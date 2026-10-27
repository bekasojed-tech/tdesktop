[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [ValidateRange(1, 64)]
    [int]$Jobs = 2,
    [ValidateRange(1, 64)]
    [int]$CompilerWorkers = 12,
    [ValidateSet('Idle', 'BelowNormal', 'Normal')]
    [string]$BuildPriority = 'Idle',
    [ValidateRange(1, 120)]
    [int]$StopTimeoutSeconds = 30,
    [switch]$CheckOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
# Use the configured spelling in folder names and messages.
$Configuration = @('Debug', 'Release') | Where-Object { $_ -ieq $Configuration }
$checkout = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$buildDirectory = Join-Path $checkout 'out'
$outputDirectory = Join-Path $buildDirectory $Configuration
# Telegram/cmake/fork_options.cmake appends this to the Telegram output folder.
$stageSuffix = '-stage'
$stageDirectory = $outputDirectory + $stageSuffix
$executable = Join-Path $outputDirectory 'Telegram.exe'
$stagedExecutable = Join-Path $stageDirectory 'Telegram.exe'
$profile = Join-Path $buildDirectory 'local-profile'
$warmUpProfile = Join-Path $buildDirectory 'rebuild-warmup'
$log = Join-Path $buildDirectory ('rebuild-{0}.log' -f $Configuration.ToLowerInvariant())
$newSuffix = '.rebuild-new'
$oldSuffix = '.rebuild-old'
$lock = $null
$exitCode = 1

. (Join-Path $PSScriptRoot 'debug_client.ps1')

function Test-StagedProject {
    $project = Join-Path $buildDirectory 'Telegram\Telegram.vcxproj'
    if (-not (Test-Path -LiteralPath $project -PathType Leaf)) {
        return $false
    }
    $expected = '>{0}$(TdesktopRebuildStage)\</OutDir>' -f $outputDirectory
    foreach ($line in [IO.File]::ReadLines($project)) {
        if ($line.Contains('<OutDir ') -and $line.EndsWith($expected, [StringComparison]::OrdinalIgnoreCase)) {
            return $true
        }
    }
    return $false
}

function Get-ChangedOutputs {
    $changed = @()
    foreach ($source in @(Get-ChildItem -LiteralPath $stageDirectory -Recurse -File)) {
        $target = Join-Path $outputDirectory $source.FullName.Substring($stageDirectory.Length + 1)
        $current = Get-Item -LiteralPath $target -ErrorAction SilentlyContinue
        if ($null -eq $current -or
            $current.Length -ne $source.Length -or
            $current.LastWriteTimeUtc -ne $source.LastWriteTimeUtc) {
            $changed += [pscustomobject]@{ Source = $source.FullName; Target = $target }
        }
    }
    return $changed
}

function Remove-NewOutputs([object[]]$Changed) {
    foreach ($item in $Changed) {
        Remove-Item -LiteralPath ($item.Target + $newSuffix) -Force -ErrorAction SilentlyContinue
    }
}

# Returns an error message when the previous files were restored and throws
# when even the restore failed.
function Install-NewOutputs([object[]]$Changed) {
    $replaced = New-Object Collections.Generic.List[object]
    try {
        foreach ($item in $Changed) {
            $entry = [pscustomobject]@{
                Target = $item.Target
                HadTarget = Test-Path -LiteralPath $item.Target -PathType Leaf
                Installed = $false
            }
            if ($entry.HadTarget) {
                [IO.File]::Move($item.Target, $item.Target + $oldSuffix)
            }
            $replaced.Add($entry)
            [IO.File]::Move($item.Target + $newSuffix, $item.Target)
            $entry.Installed = $true
        }
    } catch {
        $reason = $_.Exception.Message
        try {
            for ($i = $replaced.Count - 1; $i -ge 0; --$i) {
                $entry = $replaced[$i]
                if ($entry.Installed) {
                    [IO.File]::Delete($entry.Target)
                }
                if ($entry.HadTarget) {
                    [IO.File]::Move($entry.Target + $oldSuffix, $entry.Target)
                }
            }
        } catch {
            throw "Could not replace the client files ($reason) nor restore them ($($_.Exception.Message)). Restore the *$oldSuffix files in $outputDirectory by hand."
        }
        Remove-NewOutputs $Changed
        return "Could not replace the client files: $reason The previous files were restored."
    }
    foreach ($entry in $replaced) {
        if ($entry.HadTarget) {
            Remove-Item -LiteralPath ($entry.Target + $oldSuffix) -Force -ErrorAction SilentlyContinue
        }
    }
    return $null
}

# The antivirus scans a new EXE on its first launch, for many seconds on this
# PC, and keeps the verdict across the rename. Launching the copy with -quit
# against an empty profile pays that cost while the client still runs.
function Invoke-WarmUp([string]$Path) {
    if (-not (Test-Path -LiteralPath $warmUpProfile -PathType Container)) {
        New-Item -ItemType Directory -Path $warmUpProfile | Out-Null
    }
    $info = New-Object Diagnostics.ProcessStartInfo
    $info.FileName = $Path
    $info.Arguments = '-workdir "{0}" -noupdate -quit' -f $warmUpProfile
    $info.WorkingDirectory = $outputDirectory
    $info.UseShellExecute = $false
    $warmUp = [Diagnostics.Process]::Start($info)
    try {
        if (-not $warmUp.WaitForExit(120000)) {
            $warmUp.Kill()
            throw 'The new client did not exit its -quit warm-up within 120 seconds.'
        }
        if ($warmUp.ExitCode -ne 0) {
            throw "The new client failed its -quit warm-up (exit $($warmUp.ExitCode))."
        }
    } finally {
        $warmUp.Dispose()
    }
}

function Start-Client {
    if (-not (Test-Path -LiteralPath $profile -PathType Container)) {
        New-Item -ItemType Directory -Path $profile | Out-Null
    }
    $client = Start-Process -FilePath $executable -ArgumentList @(
        '-workdir', ('"{0}"' -f $profile), '-noupdate'
    ) -WorkingDirectory $outputDirectory -PassThru
    try {
        if ($client.WaitForExit(3000)) {
            throw "Telegram exited immediately (exit $($client.ExitCode)). Check the profile logs."
        }
        Write-Host "Telegram started (PID $($client.Id))."
    } finally {
        $client.Dispose()
    }
}

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
    if (Test-Path -LiteralPath $outputDirectory -PathType Container) {
        $interrupted = @(Get-ChildItem -LiteralPath $outputDirectory -Recurse -File -Filter "*$oldSuffix")
        if ($interrupted.Count -gt 0) {
            throw "An interrupted file replacement left $($interrupted[0].FullName). Check $outputDirectory by hand."
        }
    }
    $clients = @(Get-DebugClients $executable $profile)
    Write-Host "Checkout: $checkout"
    Write-Host "Client: $executable"
    Write-Host "Staging: $stageDirectory"
    Write-Host "Profile: $profile"
    Write-Host "Build resources: $Jobs parallel projects, at most $CompilerWorkers compiler workers in total, priority $BuildPriority."
    if ($CheckOnly) {
        if (-not (Test-StagedProject)) {
            Write-Host 'The generated project does not link into the staging folder yet; the build regenerates it first.'
        }
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
        Get-ChildItem -LiteralPath $outputDirectory -Recurse -File -Filter "*$newSuffix" -ErrorAction SilentlyContinue |
            Remove-Item -Force
        Write-Host "Building $Configuration into $stageDirectory with the client running. Log: $log"
        $buildProcess = [Diagnostics.Process]::GetCurrentProcess()
        $previousPriority = $buildProcess.PriorityClass
        try {
            $buildProcess.PriorityClass = [Diagnostics.ProcessPriorityClass]$BuildPriority
            $ErrorActionPreference = 'Continue'
            # Regenerate first, so that the build below reads the current projects.
            & $cmake --build $buildDirectory --config $Configuration --target ZERO_CHECK -- /nodeReuse:false 2>&1 |
                ForEach-Object { $_.ToString() } |
                Tee-Object -FilePath $log
            $buildExitCode = $LASTEXITCODE
            if ($buildExitCode -eq 0) {
                if (-not (Test-StagedProject)) {
                    $ErrorActionPreference = 'Stop'
                    throw 'The generated project does not link into the staging folder; check Telegram/cmake/fork_options.cmake.'
                }
                # MultiToolTask replaces /MP and caps cl.exe across all projects.
                & $cmake --build $buildDirectory --config $Configuration --target Telegram --parallel $Jobs -- /p:TdesktopRebuildStage=$stageSuffix /p:UseMultiToolTask=true /p:EnforceProcessCountAcrossBuilds=true /p:MultiProcMaxCount=$CompilerWorkers /nodeReuse:false 2>&1 |
                    ForEach-Object { $_.ToString() } |
                    Tee-Object -FilePath $log -Append
                $buildExitCode = $LASTEXITCODE
            }
        } finally {
            $ErrorActionPreference = 'Stop'
            $buildProcess.PriorityClass = $previousPriority
            $buildProcess.Dispose()
        }
        if ($buildExitCode -ne 0) {
            throw "$Configuration build failed (exit $buildExitCode). The client was not touched. See $log"
        }
        if (-not (Test-Path -LiteralPath $stagedExecutable -PathType Leaf)) {
            throw "Build completed but the staged executable is missing: $stagedExecutable"
        }
        $changed = @(Get-ChangedOutputs)
        $clients = @(Get-DebugClients $executable $profile)
        if ($changed.Count -eq 0 -and $clients.Count -gt 0) {
            Write-Host "Client files are up to date; existing Telegram left running (PID $($clients[0].Id))."
        } else {
            if ($changed.Count -gt 0) {
                # Copy while the client runs, so that it is down only for the renames.
                Write-Host "Copying $($changed.Count) changed file(s) next to the client."
                try {
                    foreach ($item in $changed) {
                        New-Item -ItemType Directory -Path (Split-Path $item.Target) -Force | Out-Null
                        [IO.File]::Copy($item.Source, $item.Target + $newSuffix, $true)
                    }
                    if (Test-Path -LiteralPath ($executable + $newSuffix) -PathType Leaf) {
                        Write-Host 'Warming up the new client before stopping the running one.'
                        Invoke-WarmUp ($executable + $newSuffix)
                    }
                    Stop-DebugClient $executable $profile $StopTimeoutSeconds
                    $downtime = [Diagnostics.Stopwatch]::StartNew()
                } catch {
                    Remove-NewOutputs $changed
                    throw
                }
                $failure = Install-NewOutputs $changed
                if ($failure) {
                    if ($clients.Count -gt 0) {
                        try {
                            Start-Client
                        } catch {
                            throw "$failure Starting the previous client failed: $($_.Exception.Message)"
                        }
                        $failure += ' The previous client was started again.'
                    }
                    throw $failure
                }
                Write-Host 'Client files replaced.'
            }
            Start-Client
            if ($changed.Count -gt 0) {
                Write-Host ('The client was down for {0:N1} seconds.' -f $downtime.Elapsed.TotalSeconds)
            }
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
