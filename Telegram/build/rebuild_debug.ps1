[CmdletBinding()]
param(
    [ValidateRange(1, 64)]
    [int]$Jobs = 6,
    [ValidateRange(1, 120)]
    [int]$StopTimeoutSeconds = 30,
    [switch]$CheckOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$checkout = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$buildDirectory = Join-Path $checkout 'out'
$executable = Join-Path $buildDirectory 'Debug\Telegram.exe'
$profile = Join-Path $buildDirectory 'local-profile'
$log = Join-Path $buildDirectory 'rebuild-debug.log'
$lock = $null
$exitCode = 1

function Test-SamePath([string]$First, [string]$Second) {
    return [IO.Path]::GetFullPath($First).TrimEnd('\', '/') -ieq `
        [IO.Path]::GetFullPath($Second).TrimEnd('\', '/')
}

function Get-ClientProfile([string]$CommandLine) {
    $match = [regex]::Match($CommandLine, '(?:^|\s)-workdir\s+(?:"([^"]+)"|(\S+))')
    if (-not $match.Success) {
        return $null
    }
    $value = $match.Groups[1].Value
    if (-not $value) {
        $value = $match.Groups[2].Value
    }
    if (-not [IO.Path]::IsPathRooted($value)) {
        return $null
    }
    return $value
}

try {
    $cacheFile = Join-Path $buildDirectory 'CMakeCache.txt'
    if (-not (Test-Path -LiteralPath $cacheFile -PathType Leaf)) {
        throw 'Configure out first; see docs/local-windows-build.md.'
    }
    $cache = @{}
    foreach ($line in [IO.File]::ReadLines($cacheFile)) {
        if ($line -match '^(CMAKE_HOME_DIRECTORY|CMAKE_GENERATOR|CMAKE_GENERATOR_INSTANCE|CMAKE_GENERATOR_PLATFORM|CMAKE_GENERATOR_TOOLSET|CMAKE_COMMAND):[^=]+=(.*)$') {
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
    $running = @(Get-CimInstance Win32_Process -Filter "Name = 'Telegram.exe'")
    $clients = @()
    foreach ($candidate in $running) {
        $candidateProfile = Get-ClientProfile $candidate.CommandLine
        if ($candidate.ExecutablePath -and (Test-SamePath $candidate.ExecutablePath $executable)) {
            if (-not $candidateProfile -or -not (Test-SamePath $candidateProfile $profile)) {
                throw "Checkout client PID $($candidate.ProcessId) uses another or unknown profile. Close it first."
            }
            $clients += Get-Process -Id $candidate.ProcessId
        } elseif ($candidateProfile -and (Test-SamePath $candidateProfile $profile)) {
            throw "Profile is used by a different executable (PID $($candidate.ProcessId)). Close it first."
        }
    }
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
        if ($clients.Count -gt 0) {
            Write-Host 'Requesting graceful shutdown with -quit...'
            $quit = Start-Process -FilePath $executable -ArgumentList @(
                '-workdir', ('"{0}"' -f $profile), '-noupdate', '-quit'
            ) -WorkingDirectory (Split-Path $executable) -WindowStyle Hidden -PassThru
            try {
                $deadline = [DateTime]::UtcNow.AddSeconds($StopTimeoutSeconds)
                foreach ($process in @($clients) + @($quit)) {
                    $remaining = [int][Math]::Max(0, ($deadline - [DateTime]::UtcNow).TotalMilliseconds)
                    if (-not $process.WaitForExit($remaining)) {
                        throw "Telegram did not exit within $StopTimeoutSeconds seconds. Close the checkout client/debugger, then retry."
                    }
                }
            } finally {
                $quit.Dispose()
            }
        }
        $stillRunning = @(Get-CimInstance Win32_Process -Filter "Name = 'Telegram.exe'" | Where-Object {
            $_.ExecutablePath -and (Test-SamePath $_.ExecutablePath $executable)
        })
        if ($stillRunning.Count -gt 0) {
            throw 'A checkout client is still running; refusing to start the build.'
        }
        Write-Host "Building Debug. Log: $log"
        $ErrorActionPreference = 'Continue'
        try {
            & $cmake --build $buildDirectory --config Debug --target Telegram --parallel $Jobs 2>&1 |
                ForEach-Object { $_.ToString() } |
                Tee-Object -FilePath $log
            $buildExitCode = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = 'Stop'
        }
        if ($buildExitCode -ne 0) {
            throw "Debug build failed (exit $buildExitCode). Client was not restarted. See $log"
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
