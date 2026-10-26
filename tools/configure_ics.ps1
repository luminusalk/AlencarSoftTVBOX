<#
.SYNOPSIS
Compartilha a internet do notebook com o receptor conectado por cabo Ethernet.
.EXAMPLE
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\configure_ics.ps1
#>
param(
    [ValidateNotNullOrEmpty()][string]$InternetAdapter = 'Wi-Fi',
    [ValidateNotNullOrEmpty()][string]$ReceiverAdapter = 'Ethernet',
    [switch]$Elevated
)
$ErrorActionPreference = 'Stop'
if ($InternetAdapter -eq $ReceiverAdapter) { throw 'Escolha dois adaptadores diferentes.' }

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    if ($Elevated) { throw 'Este PowerShell não recebeu permissão de administrador.' }
    $scriptPath = $PSCommandPath.Replace("'", "''")
    $internetName = $InternetAdapter.Replace("'", "''")
    $receiverName = $ReceiverAdapter.Replace("'", "''")
    $invocation = "& '$scriptPath' -InternetAdapter '$internetName' -ReceiverAdapter '$receiverName' -Elevated"
    $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($invocation))
    Write-Host 'Solicitando permissão de administrador ao Windows...'
    Start-Process -FilePath "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" `
        -Verb RunAs -ArgumentList @('-NoProfile', '-NoExit', '-ExecutionPolicy', 'Bypass', '-EncodedCommand', $encoded)
    return
}

$internet = Get-NetAdapter -Name $InternetAdapter -ErrorAction Stop
$receiver = Get-NetAdapter -Name $ReceiverAdapter -ErrorAction Stop
if ($internet.Status -ne 'Up' -or $receiver.Status -ne 'Up') {
    throw "Conecte os adaptadores primeiro. $InternetAdapter=$($internet.Status); $ReceiverAdapter=$($receiver.Status)."
}
if (-not (Get-NetIPConfiguration -InterfaceAlias $InternetAdapter).IPv4DefaultGateway) {
    throw "'$InternetAdapter' não possui gateway IPv4; confira sua conexão à internet."
}

$manager = New-Object -ComObject HNetCfg.HNetShare
$connections = @($manager.EnumEveryConnection() | ForEach-Object {
    $properties = $manager.NetConnectionProps($_)
    [pscustomobject]@{
        Name = [string]$properties.Name
        Configuration = $manager.INetSharingConfigurationForINetConnection($_)
    }
})
$source = $connections | Where-Object { $_.Name -eq $InternetAdapter } | Select-Object -First 1
$target = $connections | Where-Object { $_.Name -eq $ReceiverAdapter } | Select-Object -First 1
if (-not $source -or -not $target) {
    throw 'Os adaptadores não apareceram no compartilhamento do Windows.'
}
$conflicts = @($connections | Where-Object {
    $_.Name -notin @($InternetAdapter, $ReceiverAdapter) -and $_.Configuration.SharingEnabled
})
if ($conflicts.Count) {
    throw "Outro compartilhamento está ativo: $(($conflicts | ForEach-Object Name) -join ', '). Ajuste-o manualmente."
}
if ($source.Configuration.SharingEnabled -and $source.Configuration.SharingConnectionType -ne 0) {
    throw "'$InternetAdapter' está como conexão privada; ajuste-o manualmente."
}
if ($target.Configuration.SharingEnabled -and $target.Configuration.SharingConnectionType -ne 1) {
    throw "'$ReceiverAdapter' está como conexão pública; ajuste-o manualmente."
}

if (-not $source.Configuration.SharingEnabled) { $source.Configuration.EnableSharing(0) }
if (-not $target.Configuration.SharingEnabled) { $target.Configuration.EnableSharing(1) }
Start-Sleep -Seconds 3
if (-not $source.Configuration.SharingEnabled -or $source.Configuration.SharingConnectionType -ne 0 -or
    -not $target.Configuration.SharingEnabled -or $target.Configuration.SharingConnectionType -ne 1) {
    throw 'O Windows não confirmou o compartilhamento Wi-Fi → Ethernet.'
}
$addresses = @((Get-NetIPAddress -InterfaceAlias $ReceiverAdapter -AddressFamily IPv4).IPAddress)
Write-Host "Compartilhamento ativo: $InternetAdapter → $ReceiverAdapter" -ForegroundColor Green
Write-Host "Endereço Ethernet do notebook: $($addresses -join ', ')"
Write-Host 'Deixe a rede do receptor em DHCP/Automático. Ele costuma receber 192.168.137.x.'
