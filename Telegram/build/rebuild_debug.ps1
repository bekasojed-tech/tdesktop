[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [ValidateRange(1, 64)]
    [int]$Jobs = 6,
    [ValidateRange(1, 120)]
    [int]$StopTimeoutSeconds = 30,
    [switch]$CheckOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
# PRE_LINK compares the name with CMake's $<CONFIG> case-sensitively.
$Configuration = @('Debug', 'Release') | Where-Object { $_ -ieq $Configuration }
$checkout = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$buildDirectory = Join-Path $checkout 'out'
$executable = Join-Path $buildDirectory "$Configuration\Telegram.exe"
$profile = Join-Path $buildDirectory 'local-profile'
$log = Join-Path $buildDirectory ('rebuild-{0}.log' -f $Configuration.ToLowerInvariant())
$lock = $null
$exitCode = 1

. (Join-Path $PSScriptRoot 'debug_client.ps1')

try {
    $cacheFile = Join-Path $buildDirectory 'CMakeCache.txt'
    if (-not (Test-Path -LiteralPath $cacheFile -PathType Leaf)) {
        throw 'Configure out first; see docs/local-windows-build.md.'
    }
    $cache = @{}
    foreach ($line in [IO.File]::ReadLines($cacheFile)) {
        if ($line -match '^(CMAKE_HOME_DIRECTORY|CMAKE_GENERATOR|CMAKE_GENERATOR_INSTANCE|CMAKE_GENERATOR_PLATFORM|CMAKE_GENERATOR_TOOLSET|CMAKE_COMMAND|CMAKE_CONFIGURATION_TYPES):[^=]+=(.*)$') {
            $cache[$Matches[1]] = $Matches[2]
        }
    }
    if (-not $cache.ContainsKey('CMAKE_HOME_DIRECTORY') -or
        -not (Test-SamePath $cache['CMAKE_HOME_DIRECTORY'] $checkout)) {
        throw 'out belongs to another checkout; refusing to build it.'
    }
    if (-not $cache.ContainsKey('CMAKE_GENERATOR') -or
        $cache['CMAKE_GENERATOR'] -notlike 'Visual Studio *' -or
        -not $cache.ContainsKey('CMAKE_GENERATOR_INSTANCE') -or
        -not $cache.ContainsKey('CMAKE_COMMAND')) {
        throw 'This helper requires the configured native Windows Visual Studio build.'
    }
    if (-not $cache.ContainsKey('CMAKE_GENERATOR_PLATFORM') -or
        $cache['CMAKE_GENERATOR_PLATFORM'] -ne 'x64' -or
        -not $cache.ContainsKey('CMAKE_GENERATOR_TOOLSET') -or
        $cache['CMAKE_GENERATOR_TOOLSET'] -notmatch '^v143(?:,|$)') {
        throw 'This helper requires the configured x64 v143 build (MSVC 14.44).'
    }
    if (-not $cache.ContainsKey('CMAKE_CONFIGURATION_TYPES') -or
        ($cache['CMAKE_CONFIGURATION_TYPES'] -split ';') -notcontains $Configuration) {
        throw "out is not configured for $Configuration; see docs/local-client-launch.md."
    }
    $cmake = $cache['CMAKE_COMMAND']
    $vcvars = Join-Path $cache['CMAKE_GENERATOR_INSTANCE'] 'VC\Auxiliary\Build\vcvars64.bat'
    foreach ($tool in @($cmake, $vcvars)) {
        if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) {
            throw "Build tool not found: $tool"
        }
    }
    $lock = [IO.File]::Open(
        (Join-Path $buildDirectory 'rebuild-debug.lock'),
        [IO.FileMode]::OpenOrCreate,
        [IO.FileAccess]::ReadWrite,
        [IO.FileShare]::None)
    $clients = @(Get-DebugClients $executable $profile)
    Write-Host "Checkout: $checkout"
    Write-Host "Client: $executable"
    Write-Host "Profile: $profile"
    if ($CheckOnly) {
        Write-Host "Preflight OK. Matching clients: $($clients.Count). No stop, build or launch performed."
        $exitCode = 0
    } else {
        $setup = New-Object Diagnostics.ProcessStartInfo
        $setup.FileName = $env:ComSpec
        $setup.Arguments = '/d /s /c ""{0}" -vcvars_ver=14.44 >nul && set"' -f $vcvars
        $setup.UseShellExecute = $false
        $setup.CreateNoWindow = $true
        $setup.RedirectStandardOutput = $true
        $setup.RedirectStandardError = $true
        $setupProcess = [Diagnostics.Process]::Start($setup)
        try {
            $stdout = $setupProcess.StandardOutput.ReadToEndAsync()
            $stderr = $setupProcess.StandardError.ReadToEndAsync()
            $setupProcess.WaitForExit()
            if ($setupProcess.ExitCode -ne 0) {
                throw "Visual Studio environment setup failed: $($stderr.Result)"
            }
            foreach ($line in ($stdout.Result -split '\r?\n')) {
                if ($line -match '^([^=]+)=(.*)$') {
                    [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
                }
            }
        } finally {
            $setupProcess.Dispose()
        }
        $sessionFile = Join-Path $buildDirectory 'rebuild-debug-session.json'
        $runId = [Guid]::NewGuid().ToString('N')
        $session = @{
            runId = $runId
            configuration = $Configuration
            ownerProcessId = $PID
            stopTimeoutSeconds = $StopTimeoutSeconds
            preLinkCompleted = $false
        }
        [IO.File]::WriteAllText($sessionFile, ($session | ConvertTo-Json))
        $previousRun = $env:TDESKTOP_REBUILD_DEBUG_RUN
        Write-Host "Building $Configuration with the client running until PRE_LINK. Log: $log"
        try {
            $env:TDESKTOP_REBUILD_DEBUG_RUN = $runId
            $ErrorActionPreference = 'Continue'
            try {
                & $cmake --build $buildDirectory --config $Configuration --target Telegram --parallel $Jobs 2>&1 |
                    ForEach-Object { $_.ToString() } |
                    Tee-Object -FilePath $log
                $buildExitCode = $LASTEXITCODE
            } finally {
                $ErrorActionPreference = 'Stop'
            }
            $session = [IO.File]::ReadAllText($sessionFile) | ConvertFrom-Json
        } finally {
            $env:TDESKTOP_REBUILD_DEBUG_RUN = $previousRun
            Remove-Item -LiteralPath $sessionFile -ErrorAction SilentlyContinue
        }
        if ($buildExitCode -ne 0) {
            throw "$Configuration build failed (exit $buildExitCode). No restart attempted. See $log"
        }
        $clients = @(Get-DebugClients $executable $profile)
        if ($clients.Count -gt 0) {
            if ($session.preLinkCompleted) {
                throw 'A client was started during linking; close it and retry.'
            }
            Write-Host "No relink was needed; existing Telegram left running (PID $($clients[0].Id))."
            exit 0
        }
        if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
            throw "Build completed but executable is missing: $executable"
        }
        if (-not (Test-Path -LiteralPath $profile -PathType Container)) {
            New-Item -ItemType Directory -Path $profile | Out-Null
        }
        $client = Start-Process -FilePath $executable -ArgumentList @(
            '-workdir', ('"{0}"' -f $profile), '-noupdate'
        ) -WorkingDirectory (Split-Path $executable) -PassThru
        try {
            if ($client.WaitForExit(3000)) {
                throw "Rebuilt client exited immediately (exit $($client.ExitCode)). Check the profile logs."
            }
            Write-Host "Rebuilt Telegram started (PID $($client.Id))."
        } finally {
            $client.Dispose()
        }
        $exitCode = 0
    }
} catch {
    [Console]::Error.WriteLine($_.Exception.Message)
} finally {
    if ($null -ne $lock) {
        $lock.Dispose()
    }
}
exit $exitCode
