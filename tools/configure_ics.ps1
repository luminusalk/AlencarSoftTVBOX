<#
.SYNOPSIS
Compartilha a internet do notebook com o receptor conectado por cabo Ethernet,
garante o IP 192.168.137.1 na Ethernet, entrega IP via DHCP e instala os canais e o lancador nativo.
.EXAMPLE
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\configure_ics.ps1
#>
param(
    [ValidateNotNullOrEmpty()][string]$InternetAdapter = 'Wi-Fi',
    [ValidateNotNullOrEmpty()][string]$ReceiverAdapter = 'Ethernet',
    [switch]$Elevated
)
$ErrorActionPreference = 'Continue'

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    if ($Elevated) {
        Write-Host 'ERRO: Este PowerShell nao recebeu permissao de Administrador.' -ForegroundColor Red
        Read-Host 'Pressione ENTER para sair'
        exit 1
    }
    $scriptPath = $PSCommandPath.Replace("'", "''")
    $internetName = $InternetAdapter.Replace("'", "''")
    $receiverName = $ReceiverAdapter.Replace("'", "''")
    $invocation = "& '$scriptPath' -InternetAdapter '$internetName' -ReceiverAdapter '$receiverName' -Elevated"
    $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($invocation))
    Write-Host 'Solicitando permissao de administrador ao Windows...'
    Start-Process -FilePath "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" `
        -Verb RunAs -ArgumentList @('-NoProfile', '-NoExit', '-ExecutionPolicy', 'Bypass', '-EncodedCommand', $encoded)
    return
}

Write-Host '=== 1. Configurando ICS e IP 192.168.137.1 na placa Ethernet ===' -ForegroundColor Cyan
try {
    $manager = New-Object -ComObject HNetCfg.HNetShare
    $connections = @($manager.EnumEveryConnection() | ForEach-Object {
        $properties = $manager.NetConnectionProps($_)
        [pscustomobject]@{
            Name = [string]$properties.Name
            Configuration = $manager.INetSharingConfigurationForINetConnection($_)
        }
    })
    foreach ($c in $connections) {
        if ($c.Configuration.SharingEnabled) {
            $c.Configuration.DisableSharing()
        }
    }
    Start-Sleep -Seconds 2
    $source = $connections | Where-Object { $_.Name -eq $InternetAdapter } | Select-Object -First 1
    $target = $connections | Where-Object { $_.Name -eq $ReceiverAdapter } | Select-Object -First 1
    if ($source -and $target) {
        $source.Configuration.EnableSharing(0)
        $target.Configuration.EnableSharing(1)
        Start-Sleep -Seconds 2
    }
} catch {
    Write-Host "Aviso ICS COM: $($_.Exception.Message)" -ForegroundColor Yellow
}

# Garante que a placa Ethernet tenha o IP 192.168.137.1 mesmo se o ICS do Windows falhar em atribuir
$ips = @((Get-NetIPAddress -InterfaceAlias $ReceiverAdapter -AddressFamily IPv4 -ErrorAction SilentlyContinue).IPAddress)
if ($ips -notcontains '192.168.137.1') {
    Write-Host 'Forcando IP estatico 192.168.137.1 na placa Ethernet via netsh...' -ForegroundColor Yellow
    netsh interface ip set address name="$ReceiverAdapter" static 192.168.137.1 255.255.255.0
    Start-Sleep -Seconds 2
}

$ips = @((Get-NetIPAddress -InterfaceAlias $ReceiverAdapter -AddressFamily IPv4 -ErrorAction SilentlyContinue).IPAddress)
Write-Host "IP atual da placa ${ReceiverAdapter}: $($ips -join ', ')" -ForegroundColor Green

# Inicia o servidor DHCP Python para responder imediatamente com 192.168.137.2 para a box
$dhcpScript = Join-Path $PSScriptRoot 'receiver_dhcp.py'
$installScript = Join-Path $PSScriptRoot 'install_native_launcher.py'
$python = (Get-Command python.exe -ErrorAction SilentlyContinue).Source
$dhcpJob = $null
if ($python -and (Test-Path $dhcpScript)) {
    Write-Host 'Iniciando servidor DHCP (entregara 192.168.137.2 para a box)...' -ForegroundColor Cyan
    $dhcpJob = Start-Process -FilePath $python -ArgumentList "`"$dhcpScript`"" -PassThru -WindowStyle Hidden
}

Write-Host "`nAguardando a box pegar IP (tire e coloque o cabo de rede na box ou ative DHCP no menu de rede dela)..." -ForegroundColor Yellow
for ($attempt = 1; $attempt -le 30; $attempt++) {
    & $python $installScript
    if ($LASTEXITCODE -eq 0) {
        Write-Host "`n========================================================" -ForegroundColor Green
        Write-Host " TUDO APLICADO COM SUCESSO NA BOX!" -ForegroundColor Green
        Write-Host " Para entrar no PuTTY use Telnet (porta 23) no IP mostrado acima." -ForegroundColor Green
        Write-Host "========================================================" -ForegroundColor Green
        break
    }
    Start-Sleep -Seconds 3
}

if ($dhcpJob -and -not $dhcpJob.HasExited) {
    Stop-Process -Id $dhcpJob.Id -Force -ErrorAction SilentlyContinue
}
