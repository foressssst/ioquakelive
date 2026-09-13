# ioquakelive launcher
#
# Checks the private release server for a newer build, verifies the signed
# manifest, downloads only the files that changed (each one checked against its
# SHA-256), installs them and starts the game. Without a connection, or if the
# manifest does not verify, it starts the version already installed.
#
# It never downloads or touches id Software files: baseq3/pak00.pk3 comes from
# each player's own Steam install of Quake Live (the engine copies it on the
# first run, see FS_CopyFromSteam in code/qcommon/files.c).
#
# Plain ASCII on purpose: Windows PowerShell 5.1 reads BOM-less scripts as ANSI.

param(
    [switch]$NoLaunch,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$GameArgs
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$ConfigPath = Join-Path $Root 'launcher.json'
$StatePath = Join-Path $Root 'installed.json'
$LogPath = Join-Path $Root 'launcher.log'
$WorkDir = Join-Path $Root 'updates'
$GameExe = Join-Path $Root 'quakelive.x86.exe'

# Release signing key (public half). The private half never leaves the
# developer's machine.
$PublicKeyXml = '<RSAKeyValue><Modulus>uZiJn3GMJhWjL7QiH/6JD714+hsviKyCXTGKl3QFbt4hjyds/BcN/+yToU7qRruuCgGnZiTkjxRr42Z9uIZlxynOosulmVtZapC+DhhLgs24xaH1IkkOhkaCYHZovBkgt5xTmiJHe36TOsKtOY2zsnJhBSPSbTSf2JXWYal691n4JrtR536j2JKtFLVR87yQ4bEszkkX1mkEK5N44xxrD6t3YxxSaES1pThHM5RPaoq75E8tJOfgp3HYXzbS40NP0/HagSRhRsGlnnzcU5g5yMkEbcmNxJa/VADzX+hxvGjyXHMdFFVc0QE/1YekU8srMG6SifNom2IACmWGgHXgfpj3VywKyWB0IlRmswvYNUqMG3lwsraKR2c0aoxxwXm/a8CjK9t94J8PwgnXj2gS94hrRRYT33IJfvZBwiQhAE3qEuXZ1FZf3UV7jnLoRM2H47WSG5wSQuaCnAiZtpKoc3fMVKlarYcKKENsTPtBo3ow4K+Qtxnj7/GsbMTy5kXN</Modulus><Exponent>AQAB</Exponent></RSAKeyValue>'

function Write-Log([string]$Message, [string]$Color = 'Gray') {
    $line = '[{0}] {1}' -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'), $Message
    Write-Host $Message -ForegroundColor $Color
    try { Add-Content -Path $LogPath -Value $line -Encoding ASCII } catch { }
}

function New-Client($Config) {
    $wc = New-Object System.Net.WebClient
    $wc.Headers['Authorization'] = 'Basic ' + $Config.auth
    $wc.Headers['User-Agent'] = 'ioquakelive-launcher/1'
    return $wc
}

function Get-RemoteBytes($Config, [string]$Relative) {
    $wc = New-Client $Config
    try {
        # the query string keeps Cloudflare or any proxy from serving a stale manifest
        return $wc.DownloadData(('{0}/{1}?t={2}' -f $Config.baseUrl.TrimEnd('/'), $Relative, [DateTime]::UtcNow.Ticks))
    } finally { $wc.Dispose() }
}

function Save-RemoteFile($Config, [string]$Relative, [string]$Destination) {
    $wc = New-Client $Config
    try { $wc.DownloadFile(('{0}/{1}' -f $Config.baseUrl.TrimEnd('/'), $Relative), $Destination) }
    finally { $wc.Dispose() }
}

function Get-Sha256([string]$Path) {
    return (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToLowerInvariant()
}

function Test-Signature([byte[]]$Data, [byte[]]$Signature) {
    $rsa = New-Object System.Security.Cryptography.RSACryptoServiceProvider
    try {
        $rsa.FromXmlString($PublicKeyXml)
        return $rsa.VerifyData($Data, 'SHA256', $Signature)
    } finally { $rsa.Dispose() }
}

function Test-SafeRelativePath([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) { return $false }
    if ([IO.Path]::IsPathRooted($Path)) { return $false }
    if ($Path -match '(^|[\\/])\.\.([\\/]|$)') { return $false }
    if ($Path -match '[:*?"<>|]') { return $false }
    # id Software files are never part of a release
    if ([IO.Path]::GetFileName($Path) -ieq 'pak00.pk3') { return $false }
    return $true
}

function Get-VerifiedManifest($Config) {
    $channel = $Config.channel
    for ($attempt = 1; $attempt -le 2; $attempt++) {
        $manifestBytes = Get-RemoteBytes $Config "$channel/manifest.json"
        $signature = Get-RemoteBytes $Config "$channel/manifest.json.sig"
        if (Test-Signature $manifestBytes $signature) {
            $text = [Text.Encoding]::UTF8.GetString($manifestBytes)
            return ($text | ConvertFrom-Json)
        }
        # A publish swaps manifest and signature back to back; retry once.
        Start-Sleep -Seconds 2
    }
    throw 'La firma del manifiesto NO es valida. No se instala nada.'
}

function Start-Game([string]$Version, $ManifestArgs) {
    if ($NoLaunch) { Write-Log 'Modo -NoLaunch: no se abre el juego.'; return }
    if (-not (Test-Path -LiteralPath $GameExe)) {
        Write-Log 'El juego no esta instalado todavia y no hay conexion con el servidor.' 'Red'
        exit 1
    }
    $launchArgs = @()
    if ($ManifestArgs) { $launchArgs += $ManifestArgs }
    if ($GameArgs) { $launchArgs += $GameArgs }
    Write-Log ("Abriendo ioquakelive {0}..." -f $Version) 'Green'
    if ($launchArgs.Count -gt 0) {
        Start-Process -FilePath $GameExe -ArgumentList $launchArgs -WorkingDirectory $Root | Out-Null
    } else {
        Start-Process -FilePath $GameExe -WorkingDirectory $Root | Out-Null
    }
}

# ---------------------------------------------------------------------------

Write-Host ''
Write-Host '  ioquakelive launcher' -ForegroundColor Cyan
Write-Host ''

if (-not (Test-Path -LiteralPath $ConfigPath)) {
    Write-Log "Falta $ConfigPath. Reinstala con el comando de instalacion." 'Red'
    exit 1
}
$config = Get-Content -LiteralPath $ConfigPath -Raw | ConvertFrom-Json

$state = $null
if (Test-Path -LiteralPath $StatePath) {
    try { $state = Get-Content -LiteralPath $StatePath -Raw | ConvertFrom-Json } catch { $state = $null }
}
$installedVersion = if ($state) { $state.version } else { '(ninguna)' }

try {
    Write-Log ("Buscando actualizaciones (canal {0})..." -f $config.channel)
    $manifest = Get-VerifiedManifest $config
} catch {
    Write-Log ("No se pudo comprobar actualizaciones: {0}" -f $_.Exception.Message) 'Yellow'
    Start-Game $installedVersion $null
    exit 0
}

if ($manifest.format -ne 1 -or $manifest.channel -ne $config.channel) {
    Write-Log 'Manifiesto de un formato o canal inesperado; se abre la version instalada.' 'Yellow'
    Start-Game $installedVersion $null
    exit 0
}

# Which files differ from the signed manifest
$pending = @()
foreach ($file in $manifest.files) {
    if (-not (Test-SafeRelativePath $file.path)) { throw ("Ruta no permitida en el manifiesto: {0}" -f $file.path) }
    $local = Join-Path $Root $file.path
    $needs = $true
    if (Test-Path -LiteralPath $local) {
        if ((Get-Item -LiteralPath $local).Length -eq [int64]$file.size) {
            $needs = ((Get-Sha256 $local) -ne $file.sha256.ToLowerInvariant())
        }
    }
    if ($needs) { $pending += $file }
}

if ($pending.Count -eq 0) {
    Write-Log ("Todo al dia: {0}" -f $manifest.version) 'Green'
} else {
    Write-Log ("Actualizando {0} -> {1} ({2} archivo(s))" -f $installedVersion, $manifest.version, $pending.Count) 'Cyan'
    if ($manifest.notes) { Write-Log ("Notas: {0}" -f $manifest.notes) }

    $running = Get-Process -Name 'quakelive.x86' -ErrorAction SilentlyContinue |
        Where-Object { $_.Path -and $_.Path.StartsWith($Root, [StringComparison]::OrdinalIgnoreCase) }
    if ($running) {
        Write-Log 'El juego esta abierto. Cierralo y vuelve a abrir el launcher.' 'Red'
        Read-Host 'Presiona Enter para salir'
        exit 1
    }

    New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null
    foreach ($file in $pending) {
        $part = Join-Path $WorkDir ($file.sha256 + '.part')
        Write-Log ("  bajando {0} ({1:N0} KB)" -f $file.path, ([int64]$file.size / 1KB))
        Save-RemoteFile $config ("{0}/files/{1}" -f $config.channel, $file.sha256) $part
        if ((Get-Item -LiteralPath $part).Length -ne [int64]$file.size -or (Get-Sha256 $part) -ne $file.sha256.ToLowerInvariant()) {
            Remove-Item -LiteralPath $part -Force
            throw ("Descarga corrupta o alterada: {0}. No se instalo nada." -f $file.path)
        }
    }

    # Everything downloaded and verified: install
    foreach ($file in $pending) {
        $part = Join-Path $WorkDir ($file.sha256 + '.part')
        $local = Join-Path $Root $file.path
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $local) | Out-Null
        Move-Item -LiteralPath $part -Destination $local -Force
    }
}

# Remove files an older release installed that this one no longer ships
if ($state -and $state.files) {
    $keep = @{}
    foreach ($file in $manifest.files) { $keep[$file.path.ToLowerInvariant()] = $true }
    foreach ($old in $state.files) {
        if ((Test-SafeRelativePath $old) -and -not $keep.ContainsKey($old.ToLowerInvariant())) {
            $oldPath = Join-Path $Root $old
            if (Test-Path -LiteralPath $oldPath) {
                Write-Log ("  quitando {0} (ya no forma parte de la version)" -f $old)
                Remove-Item -LiteralPath $oldPath -Force
            }
        }
    }
}

$newState = [ordered]@{
    version = $manifest.version
    commit  = $manifest.commit
    updated = (Get-Date).ToString('s')
    files   = @($manifest.files | ForEach-Object { $_.path })
}
($newState | ConvertTo-Json -Depth 4) | Set-Content -LiteralPath $StatePath -Encoding ASCII
if (Test-Path -LiteralPath $WorkDir) { Remove-Item -LiteralPath $WorkDir -Recurse -Force -ErrorAction SilentlyContinue }

Start-Game $manifest.version $manifest.gameArgs
