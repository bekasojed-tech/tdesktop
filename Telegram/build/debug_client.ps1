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

function Get-DebugClients([string]$Executable, [string]$Profile) {
    $running = @(Get-CimInstance Win32_Process -Filter "Name = 'Telegram.exe'")
    $clients = @()
    foreach ($candidate in $running) {
        $candidateProfile = Get-ClientProfile $candidate.CommandLine
        if ($candidate.ExecutablePath -and (Test-SamePath $candidate.ExecutablePath $Executable)) {
            if (-not $candidateProfile -or -not (Test-SamePath $candidateProfile $Profile)) {
                throw "Checkout client PID $($candidate.ProcessId) uses another or unknown profile. Close it first."
            }
            $clients += Get-Process -Id $candidate.ProcessId
        } elseif ($candidateProfile -and (Test-SamePath $candidateProfile $Profile)) {
            throw "Profile is used by a different executable (PID $($candidate.ProcessId)). Close it first."
        }
    }
    return $clients
}

function Stop-DebugClient([string]$Executable, [string]$Profile, [int]$TimeoutSeconds) {
    $clients = @(Get-DebugClients $Executable $Profile)
    if ($clients.Count -gt 0) {
        Write-Host 'Requesting graceful shutdown with -quit...'
        $quit = Start-Process -FilePath $Executable -ArgumentList @(
            '-workdir', ('"{0}"' -f $Profile), '-noupdate', '-quit'
        ) -WorkingDirectory (Split-Path $Executable) -WindowStyle Hidden -PassThru
        try {
            $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
            foreach ($process in @($clients) + @($quit)) {
                $remaining = [int][Math]::Max(0, ($deadline - [DateTime]::UtcNow).TotalMilliseconds)
                if (-not $process.WaitForExit($remaining)) {
                    throw "Telegram did not exit within $TimeoutSeconds seconds. Close the checkout client/debugger, then retry."
                }
            }
        } finally {
            $quit.Dispose()
        }
    }
    if (@(Get-DebugClients $Executable $Profile).Count -gt 0) {
        throw 'A checkout client is still running; refusing to replace its files.'
    }
}
