# ioquakelive launcher
#
# Checks the private release server for a newer build, verifies the signed
# manifest, downloads only the files that changed (each one checked against its
# SHA-256), installs them and starts the game. Without a connection, or if the
# manifest does not verify, it starts the version already installed.
#
# It never downloads or touches id Software files: baseq3/pak00.pk3 comes from
# each player's own Steam install of Quake Live: the launcher links or copies it
# on the first run, as FS_CopyFromSteam (code/qcommon/files.c) does when the
# game is started directly. Everything ioquakelive adds, CPMA's animation frames
# and the frag message's font included, travels in baseq3/pak01.pk3.
# Settings are never taken from Steam: ioquakelive keeps its own, with the
# player's screenshots, demos and fonts, in %APPDATA%\ioquakelive.
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
$UserDataDir = Join-Path $env:APPDATA 'ioquakelive'   # the game's fs_homepath

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
    if ($Config.auth) { $wc.Headers['Authorization'] = 'Basic ' + $Config.auth }
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

# ---------------------------------------------------------------------------
# Quake Live data from the player's Steam install (what FS_CopyFromSteam does
# in the engine, done here first so it can show progress and a clear message).

$Pak00Size = 962052238

function Find-SteamQuakeLive {
    $libs = @()
    foreach ($key in @('HKCU:\Software\Valve\Steam', 'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam', 'HKLM:\SOFTWARE\Valve\Steam')) {
        try {
            $p = Get-ItemProperty -Path $key -ErrorAction Stop
            foreach ($v in @($p.SteamPath, $p.InstallPath)) { if ($v) { $libs += ($v -replace '/', '\') } }
        } catch { }
    }
    foreach ($steam in @($libs)) {
        $vdf = Join-Path $steam 'steamapps\libraryfolders.vdf'
        if (Test-Path -LiteralPath $vdf) {
            foreach ($m in [regex]::Matches((Get-Content -LiteralPath $vdf -Raw), '"(?:path|\d+)"\s+"([^"]+)"')) {
                $libs += ($m.Groups[1].Value -replace '\\\\', '\')
            }
        }
    }
    foreach ($lib in ($libs | Select-Object -Unique)) {
        $dir = Join-Path $lib 'steamapps\common\Quake Live'
        $pak = Join-Path $dir 'baseq3\pak00.pk3'
        if ((Test-Path -LiteralPath $pak) -and (Get-Item -LiteralPath $pak).Length -eq $Pak00Size) { return $dir }
    }
    return $null
}

function Copy-WithProgress([string]$Source, [string]$Destination) {
    $ProgressPreference = 'Continue'
    $tmp = $Destination + '.part'
    $in = [IO.File]::OpenRead($Source)
    try {
        $out = [IO.File]::Create($tmp)
        try {
            $buf = New-Object byte[] (4MB)
            $done = 0L; $last = -1
            while (($n = $in.Read($buf, 0, $buf.Length)) -gt 0) {
                $out.Write($buf, 0, $n); $done += $n
                $pct = [int](100 * $done / $in.Length)
                if ($pct -ne $last) { Write-Progress -Activity 'Copiando archivos de Quake Live' -Status "$pct %" -PercentComplete $pct; $last = $pct }
            }
        } finally { $out.Dispose() }
    } finally { $in.Dispose() }
    Write-Progress -Activity 'Copiando archivos de Quake Live' -Completed
    Move-Item -LiteralPath $tmp -Destination $Destination -Force
}

function Initialize-QuakeLiveData {
    $target = Join-Path $Root 'baseq3\pak00.pk3'
    if ((Test-Path -LiteralPath $target) -and (Get-Item -LiteralPath $target).Length -eq $Pak00Size) { return $true }

    $ql = Find-SteamQuakeLive
    if (-not $ql) {
        Write-Log 'No encontre Quake Live en Steam.' 'Red'
        Write-Log 'ioquakelive necesita Quake Live comprado e instalado desde Steam. Abriendo Steam...' 'Yellow'
        try { Start-Process 'steam://install/282440' } catch { }
        return $false
    }
    Write-Log ("Quake Live encontrado en Steam: {0}" -f $ql) 'Green'

    $source = Join-Path $ql 'baseq3\pak00.pk3'
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
    if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Force }
    try {
        New-Item -ItemType HardLink -Path $target -Value $source -ErrorAction Stop | Out-Null
        Write-Log 'Archivos de Quake Live listos (vinculados, sin copiar).'
    } catch {
        Write-Log 'Copiando archivos de Quake Live (917 MB, solo la primera vez)...' 'Cyan'
        Copy-WithProgress $source $target
    }
    return $true
}

# The player's data used to share upstream's %APPDATA%\quakelive. The first run that finds
# that folder and not %APPDATA%\ioquakelive renames it, as the game does (Sys_DefaultHomePath),
# before anything else writes to the new one.
function Move-UpstreamUserData {
    $old = Join-Path $env:APPDATA 'quakelive'
    if ((Test-Path -LiteralPath $UserDataDir) -or -not (Test-Path -LiteralPath $old -PathType Container)) { return }
    try {
        Move-Item -LiteralPath $old -Destination $UserDataDir
        Write-Log ("Tus configs, screenshots y demos ahora estan en {0}" -f $UserDataDir) 'Green'
    } catch {
        Write-Log ("No se pudo mover {0}: {1}. El juego lo vuelve a intentar al abrir." -f $old, $_.Exception.Message) 'Yellow'
    }
}

# ---------------------------------------------------------------------------
# Until 2026-09-13 the launcher made two extras on each PC: zz-cpma-anim.pk3 with CPMA's
# animation frames, and a copy of the player's Handel Gothic Ex in baseq3/fonts. Both now
# ship in pak01.pk3, so the extra pak goes, and so does the font copy when it is the same
# file pak01 carries (any other font in that folder is the player's and stays).

function Remove-OldExtras {
    foreach ($leftover in @('baseq3\zz-cpma-anim.pk3', 'baseq3\zz-cpma-anim.pk3.part', 'cpma-1.53-nomaps.zip')) {
        $path = Join-Path $Root $leftover
        if (-not (Test-Path -LiteralPath $path)) { continue }
        try {
            Remove-Item -LiteralPath $path -Force
            Write-Log ("  quitando {0} (ahora viene en pak01.pk3)" -f $leftover)
        } catch {
            Write-Log ("No se pudo borrar {0}: {1}" -f $path, $_.Exception.Message) 'Yellow'
        }
    }

    $font = Join-Path $UserDataDir 'baseq3\fonts\handelgothicex.ttf'
    $pak01 = Join-Path $Root 'baseq3\pak01.pk3'
    if (-not (Test-Path -LiteralPath $font) -or -not (Test-Path -LiteralPath $pak01)) { return }
    try {
        Add-Type -AssemblyName System.IO.Compression
        $shipped = $null
        $stream = [IO.File]::OpenRead($pak01)
        try {
            $zip = New-Object IO.Compression.ZipArchive($stream, [IO.Compression.ZipArchiveMode]::Read)
            $entry = $zip.GetEntry('fonts/handelgothicex.ttf')
            if ($entry) {
                $bytes = New-Object IO.MemoryStream
                $in = $entry.Open()
                try { $in.CopyTo($bytes) } finally { $in.Dispose() }
                $sha = [Security.Cryptography.SHA256]::Create()
                try { $shipped = -join ($sha.ComputeHash($bytes.ToArray()) | ForEach-Object { $_.ToString('x2') }) } finally { $sha.Dispose() }
            }
            $zip.Dispose()
        } finally { $stream.Dispose() }
        if ($shipped -and (Get-Sha256 $font) -eq $shipped) {
            Remove-Item -LiteralPath $font -Force
            Write-Log '  quitando la copia de Handel Gothic Ex (ahora viene en pak01.pk3)'
        }
    } catch {
        Write-Log ("No se pudo revisar la copia de Handel Gothic Ex: {0}" -f $_.Exception.Message) 'Yellow'
    }
}

function Start-Game([string]$Version, $ManifestArgs) {
    if (-not (Initialize-QuakeLiveData)) {
        Read-Host 'Presiona Enter para salir'
        exit 1
    }
    Move-UpstreamUserData
    Remove-OldExtras
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
