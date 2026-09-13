# ioquakelive installer (bootstrap)
#
# Started by ioquakelive-setup.cmd, which sets $IoqlBase (the private release
# URL) before downloading this file. Optional: set $IoqlDir first to choose the
# install folder (default %LOCALAPPDATA%\ioquakelive).
#
# It writes launcher.json, fetches the signed manifest, verifies it with the
# release public key, installs launcher.ps1 and ioquakelive.cmd (checking their
# SHA-256), creates a desktop shortcut and runs the launcher, which downloads the
# game. Quake Live must be installed from Steam: the launcher takes pak00.pk3
# from there on its first run.
#
# Plain ASCII on purpose (it is run through Invoke-Expression).

& {
    $ErrorActionPreference = 'Stop'
    $ProgressPreference = 'SilentlyContinue'
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

    $channel = 'beta'
    $publicKeyXml = '<RSAKeyValue><Modulus>uZiJn3GMJhWjL7QiH/6JD714+hsviKyCXTGKl3QFbt4hjyds/BcN/+yToU7qRruuCgGnZiTkjxRr42Z9uIZlxynOosulmVtZapC+DhhLgs24xaH1IkkOhkaCYHZovBkgt5xTmiJHe36TOsKtOY2zsnJhBSPSbTSf2JXWYal691n4JrtR536j2JKtFLVR87yQ4bEszkkX1mkEK5N44xxrD6t3YxxSaES1pThHM5RPaoq75E8tJOfgp3HYXzbS40NP0/HagSRhRsGlnnzcU5g5yMkEbcmNxJa/VADzX+hxvGjyXHMdFFVc0QE/1YekU8srMG6SifNom2IACmWGgHXgfpj3VywKyWB0IlRmswvYNUqMG3lwsraKR2c0aoxxwXm/a8CjK9t94J8PwgnXj2gS94hrRRYT33IJfvZBwiQhAE3qEuXZ1FZf3UV7jnLoRM2H47WSG5wSQuaCnAiZtpKoc3fMVKlarYcKKENsTPtBo3ow4K+Qtxnj7/GsbMTy5kXN</Modulus><Exponent>AQAB</Exponent></RSAKeyValue>'

    if (-not (Get-Variable -Name IoqlBase -Scope Global -ErrorAction SilentlyContinue)) {
        throw 'Abre ioquakelive-setup.cmd para instalar.'
    }
    $baseUrl = $global:IoqlBase.TrimEnd('/')
    $dir = if (Get-Variable -Name IoqlDir -Scope Global -ErrorAction SilentlyContinue) { $global:IoqlDir } else { Join-Path $env:LOCALAPPDATA 'ioquakelive' }

    Write-Host ''
    Write-Host '  Instalando ioquakelive en' $dir -ForegroundColor Cyan
    Write-Host ''

    # Quake Live on Steam is a requirement (pak00.pk3 comes from there)
    $steam = $null
    try { $steam = (Get-ItemProperty -Path 'HKCU:\Software\Valve\Steam' -ErrorAction Stop).SteamPath } catch { }
    if (-not $steam) {
        Write-Host 'No encontre Steam. ioquakelive necesita Quake Live comprado e instalado desde Steam.' -ForegroundColor Red
        try { Start-Process 'steam://install/282440' } catch { }
        throw 'Instala Quake Live desde Steam y vuelve a abrir el instalador.'
    }

    New-Item -ItemType Directory -Force -Path $dir | Out-Null

    function Get-Bytes([string]$rel) {
        $wc = New-Object System.Net.WebClient
        $wc.Headers['User-Agent'] = 'ioquakelive-installer/1'
        try { return $wc.DownloadData(('{0}/{1}?t={2}' -f $baseUrl, $rel, [DateTime]::UtcNow.Ticks)) } finally { $wc.Dispose() }
    }

    $manifestBytes = Get-Bytes "$channel/manifest.json"
    $signature = Get-Bytes "$channel/manifest.json.sig"
    $rsa = New-Object System.Security.Cryptography.RSACryptoServiceProvider
    $rsa.FromXmlString($publicKeyXml)
    if (-not $rsa.VerifyData($manifestBytes, 'SHA256', $signature)) { throw 'La firma del manifiesto NO es valida. Instalacion cancelada.' }
    $rsa.Dispose()
    $manifest = [Text.Encoding]::UTF8.GetString($manifestBytes) | ConvertFrom-Json

    foreach ($name in @('launcher.ps1', 'ioquakelive.cmd')) {
        $entry = $manifest.files | Where-Object { $_.path -eq $name } | Select-Object -First 1
        if (-not $entry) { throw "El manifiesto no trae $name" }
        $bytes = Get-Bytes ("{0}/files/{1}" -f $channel, $entry.sha256)
        $sha = New-Object System.Security.Cryptography.SHA256Managed
        $hash = -join ($sha.ComputeHash($bytes) | ForEach-Object { $_.ToString('x2') })
        if ($hash -ne $entry.sha256.ToLowerInvariant()) { throw "Descarga alterada: $name" }
        [IO.File]::WriteAllBytes((Join-Path $dir $name), $bytes)
    }

    $config = [ordered]@{ baseUrl = $baseUrl; channel = $channel; auth = '' }
    ($config | ConvertTo-Json) | Set-Content -LiteralPath (Join-Path $dir 'launcher.json') -Encoding ASCII

    try {
        $shell = New-Object -ComObject WScript.Shell
        $link = $shell.CreateShortcut((Join-Path ([Environment]::GetFolderPath('Desktop')) 'ioquakelive.lnk'))
        $link.TargetPath = Join-Path $dir 'ioquakelive.cmd'
        $link.WorkingDirectory = $dir
        $link.IconLocation = (Join-Path $dir 'quakelive.x86.exe') + ',0'
        $link.Save()
        Write-Host 'Acceso directo "ioquakelive" creado en el escritorio.' -ForegroundColor Green
    } catch {
        Write-Host 'No se pudo crear el acceso directo; abre ioquakelive.cmd desde la carpeta.' -ForegroundColor Yellow
    }

    Write-Host ''
    if (Get-Variable -Name IoqlNoLaunch -Scope Global -ErrorAction SilentlyContinue) {
        # testing: install and update, but do not start the game
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $dir 'launcher.ps1') -NoLaunch
    } else {
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $dir 'launcher.ps1')
    }
}
