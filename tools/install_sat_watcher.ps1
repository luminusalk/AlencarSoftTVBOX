<#
.SYNOPSIS
Starts the SAT watcher and makes it run at notebook login. No admin required.
#>
$ErrorActionPreference = 'Stop'
$script = Join-Path $PSScriptRoot 'sat_boot_watcher.py'
if (-not (Test-Path -LiteralPath $script)) { throw "Missing $script" }
$python = (Get-Command pythonw.exe -ErrorAction Stop).Source
$runKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$value = '"' + $python + '" "' + $script + '"'
New-ItemProperty -Path $runKey -Name 'AlencarSoftTVBOX-SAT' -Value $value -PropertyType String -Force | Out-Null
$running = Get-CimInstance Win32_Process | Where-Object { $_.Name -eq 'pythonw.exe' -and $_.CommandLine -like '*sat_boot_watcher.py*' }
if (-not $running) {
    Start-Process -FilePath $python -ArgumentList ('"' + $script + '"') -WindowStyle Hidden
}
Write-Host 'SAT monitor active. It will restart at notebook login and reconnect after receiver reboot.'
Write-Host 'Log: %LOCALAPPDATA%\AlencarSoftTVBOX\sat_boot_watcher.log'
