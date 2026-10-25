$ErrorActionPreference = 'Stop'
$resultPath = 'D:\EMULADORES\Git\REA\research\azamerica_probe\ics_result.json'
$result = [ordered]@{
    time = (Get-Date).ToString('o')
    success = $false
    message = ''
    sharing = @()
    ethernetIPv4 = @()
}

try {
    $wifiAdapter = Get-NetAdapter -Name 'Wi-Fi'
    $ethernetAdapter = Get-NetAdapter -Name 'Ethernet'
    if ($wifiAdapter.Status -ne 'Up' -or $ethernetAdapter.Status -ne 'Up') {
        throw 'Wi-Fi e Ethernet precisam estar conectados antes de ativar o compartilhamento.'
    }
    $wifiConfig = Get-NetIPConfiguration -InterfaceAlias 'Wi-Fi'
    if (-not $wifiConfig.IPv4DefaultGateway) {
        throw 'O Wi-Fi não tem gateway IPv4; verifique a conexão com a internet.'
    }

    $manager = New-Object -ComObject HNetCfg.HNetShare
    $connections = @($manager.EnumEveryConnection() | ForEach-Object {
        $properties = $manager.NetConnectionProps($_)
        [pscustomobject]@{
            Name = $properties.Name
            Configuration = $manager.INetSharingConfigurationForINetConnection($_)
        }
    })
    $wifi = $connections | Where-Object Name -eq 'Wi-Fi' | Select-Object -First 1
    $ethernet = $connections | Where-Object Name -eq 'Ethernet' | Select-Object -First 1
    if (-not $wifi -or -not $ethernet) {
        throw 'Não encontrei as conexões Wi-Fi e Ethernet no serviço de compartilhamento.'
    }

    $otherSharing = @($connections | Where-Object {
        $_.Name -notin @('Wi-Fi', 'Ethernet') -and $_.Configuration.SharingEnabled
    })
    if ($otherSharing.Count -gt 0) {
        throw ('Outra conexão já compartilha a rede: ' + (($otherSharing | ForEach-Object Name) -join ', '))
    }

    if (-not $wifi.Configuration.SharingEnabled -or $wifi.Configuration.SharingConnectionType -ne 0) {
        $wifi.Configuration.EnableSharing(0)
    }
    if (-not $ethernet.Configuration.SharingEnabled -or $ethernet.Configuration.SharingConnectionType -ne 1) {
        $ethernet.Configuration.EnableSharing(1)
    }

    Start-Sleep -Seconds 4
    $result.sharing = @($connections | Where-Object Name -in @('Wi-Fi', 'Ethernet') | ForEach-Object {
        [ordered]@{
            name = $_.Name
            enabled = [bool]$_.Configuration.SharingEnabled
            type = [int]$_.Configuration.SharingConnectionType
        }
    })
    $result.ethernetIPv4 = @(Get-NetIPAddress -InterfaceAlias 'Ethernet' -AddressFamily IPv4 | ForEach-Object IPAddress)
    if (-not ($result.sharing | Where-Object { $_.name -eq 'Wi-Fi' -and $_.enabled -and $_.type -eq 0 })) {
        throw 'O compartilhamento de Wi-Fi não ficou ativo.'
    }
    if (-not ($result.sharing | Where-Object { $_.name -eq 'Ethernet' -and $_.enabled -and $_.type -eq 1 })) {
        throw 'O compartilhamento de Ethernet não ficou ativo.'
    }
    $result.success = $true
    $result.message = 'Wi-Fi compartilhado com Ethernet.'
} catch {
    $result.message = $_.Exception.Message
} finally {
    $result | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $resultPath -Encoding UTF8
}
