<#
.SYNOPSIS
Compartilha a internet do notebook com o receptor conectado por cabo Ethernet,
entrega IP via DHCP e instala automaticamente a lista de 274 canais e o lancador nativo.
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
if ($target.Configuration.SharingEnabled) { $target.Configuration.DisableSharing() }
if ($source.Configuration.SharingEnabled) { $source.Configuration.DisableSharing() }
Start-Sleep -Seconds 2
$source.Configuration.EnableSharing(0)
$target.Configuration.EnableSharing(1)
Start-Sleep -Seconds 2

# Reinicia fisicamente o link da placa Ethernet para forcar a box a pedir DHCP imediatamente
Write-Host "Reiniciando link da placa $ReceiverAdapter para a box renovar o DHCP automaticamente..." -ForegroundColor Cyan
Restart-NetAdapter -Name $ReceiverAdapter
Start-Sleep -Seconds 4

$addresses = @((Get-NetIPAddress -InterfaceAlias $ReceiverAdapter -AddressFamily IPv4 -ErrorAction SilentlyContinue).IPAddress)
Write-Host "Compartilhamento ativo: $InternetAdapter → $ReceiverAdapter" -ForegroundColor Green
Write-Host "Endereço Ethernet do notebook: $($addresses -join ', ')"

# Inicia o servidor DHCP fallback em background caso o ICS do Windows atrase o DHCP Offer
$dhcpScript = Join-Path $PSScriptRoot 'receiver_dhcp.py'
$installScript = Join-Path $PSScriptRoot 'install_native_launcher.py'
$python = (Get-Command python.exe -ErrorAction SilentlyContinue).Source
if ($python -and (Test-Path $dhcpScript)) {
    $dhcpJob = Start-Process -FilePath $python -ArgumentList "`"$dhcpScript`"" -PassThru -WindowStyle Hidden
}

Write-Host "Aguardando o receptor responder na rede 192.168.137.x (se necessario, tire e coloque o cabo de rede na box)..." -ForegroundColor Yellow
for ($attempt = 1; $attempt -le 20; $attempt++) {
    & $python $installScript
    if ($LASTEXITCODE -eq 0) {
        Write-Host "`nTUDO APLICADO COM SUCESSO NA BOX (274 Canais + Lancador Nativo Sem Notebook)!" -ForegroundColor Green
        break
    }
    Start-Sleep -Seconds 3
}

if ($dhcpJob -and -not $dhcpJob.HasExited) {
    Stop-Process -Id $dhcpJob.Id -Force -ErrorAction SilentlyContinue
}
