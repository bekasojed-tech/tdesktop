[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDirectory,
    [Parameter(Mandatory = $true)]
    [string]$Configuration,
    [Parameter(Mandatory = $true)]
    [string]$Executable
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if (-not $env:TDESKTOP_REBUILD_DEBUG_RUN) {
    exit 0
}

try {
    . (Join-Path $PSScriptRoot 'debug_client.ps1')
    $checkout = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
    if ($Configuration -ne 'Debug' -or
        -not (Test-SamePath $BuildDirectory (Join-Path $checkout 'out')) -or
        -not (Test-SamePath $Executable (Join-Path $checkout 'out\Debug\Telegram.exe'))) {
        throw 'PRE_LINK rebuild request does not match this checkout Debug build.'
    }
    $sessionFile = Join-Path $BuildDirectory 'rebuild-debug-session.json'
    $session = [IO.File]::ReadAllText($sessionFile) | ConvertFrom-Json
    if ($session.runId -cne $env:TDESKTOP_REBUILD_DEBUG_RUN -or
        $session.stopTimeoutSeconds -lt 1 -or $session.stopTimeoutSeconds -gt 120) {
        throw 'PRE_LINK rebuild request is invalid or stale.'
    }
    $owner = Get-Process -Id $session.ownerProcessId
    $owner.Dispose()
    Stop-DebugClient $Executable (Join-Path $BuildDirectory 'local-profile') $session.stopTimeoutSeconds
    $session.preLinkCompleted = $true
    [IO.File]::WriteAllText($sessionFile, ($session | ConvertTo-Json))
    Write-Host 'PRE_LINK: checkout client has exited; linking may proceed.'
    exit 0
} catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
}
