$ErrorActionPreference = 'Stop'
$runKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
Remove-ItemProperty -Path $runKey -Name 'AlencarSoftTVBOX-SAT' -ErrorAction SilentlyContinue
Get-CimInstance Win32_Process |
    Where-Object { $_.Name -eq 'pythonw.exe' -and $_.CommandLine -like '*sat_boot_watcher.py*' } |
    ForEach-Object { Stop-Process -Id $_.ProcessId -ErrorAction Stop }
Write-Host 'SAT monitor removed from notebook startup.'
