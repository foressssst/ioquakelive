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
# game is started directly. CPMA's animation frames get the same treatment: they
# are extracted on each machine from the official CPMA package, never shipped.
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

    # The player's Quake Live settings, only where none exist yet
    $cfgHome = Join-Path $env:APPDATA 'quakelive\baseq3'
    $userDir = Get-ChildItem -LiteralPath $ql -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^\d{10,}$' -and (Test-Path -LiteralPath (Join-Path $_.FullName 'baseq3')) } |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($userDir) {
        foreach ($cfg in @('qzconfig.cfg', 'repconfig.cfg', 'autoexec.cfg')) {
            $src = Join-Path $userDir.FullName ('baseq3\' + $cfg)
            $dst = Join-Path $cfgHome $cfg
            if ((Test-Path -LiteralPath $src) -and -not (Test-Path -LiteralPath $dst)) {
                New-Item -ItemType Directory -Force -Path $cfgHome | Out-Null
                Copy-Item -LiteralPath $src -Destination $dst
                Write-Log ("  tu configuracion de Quake Live: {0}" -f $cfg)
            }
        }
    }
    return $true
}

# ---------------------------------------------------------------------------
# CPMA 1.53 animation frames (medals, chat balloon). Like pak00.pk3 they are never
# part of a release: they come from the official CPMA package, downloaded from its
# own site and checked against its SHA-256, and only the 160 frames are kept.

$CpmaZipUrl = 'https://cdn.playmorepromode.com/files/cpma/cpma-1.53-nomaps.zip'
$CpmaZipSha256 = 'edfffa0c1a0375ba46a5b42257a168fb15086712245733526ab2d9ccdd821ca0'
$CpmaFrames = '^(menu/medals/(assist|capture|defend|excellent|gauntlet|impressive)/\d+\.tga|sprites/chat\d\d\.tga)$'

function Initialize-CpmaAnimations {
    $target = Join-Path $Root 'baseq3\zz-cpma-anim.pk3'
    if (Test-Path -LiteralPath $target) { return }

    $zipPath = Join-Path $Root 'cpma-1.53-nomaps.zip'
    $part = $target + '.part'
    try {
        Add-Type -AssemblyName System.IO.Compression
        Write-Log 'Bajando CPMA 1.53 de su sitio oficial para las animaciones (17 MB, solo la primera vez)...' 'Cyan'
        $wc = New-Object System.Net.WebClient
        $wc.Headers['User-Agent'] = 'ioquakelive-launcher/1'
        try { $wc.DownloadFile($CpmaZipUrl, $zipPath) } finally { $wc.Dispose() }
        if ((Get-Sha256 $zipPath) -ne $CpmaZipSha256) { throw 'el paquete de CPMA no es la version esperada' }

        $count = 0
        $zipStream = [IO.File]::OpenRead($zipPath)
        try {
            $zip = New-Object IO.Compression.ZipArchive($zipStream, [IO.Compression.ZipArchiveMode]::Read)
            $pk3Bytes = New-Object IO.MemoryStream
            $in = $zip.GetEntry('cpma/z-cpma-pak153.pk3').Open()
            try { $in.CopyTo($pk3Bytes) } finally { $in.Dispose() }
            $pk3Bytes.Position = 0
            $pk3 = New-Object IO.Compression.ZipArchive($pk3Bytes, [IO.Compression.ZipArchiveMode]::Read)

            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
            $outStream = [IO.File]::Create($part)
            try {
                $out = New-Object IO.Compression.ZipArchive($outStream, [IO.Compression.ZipArchiveMode]::Create)
                foreach ($entry in $pk3.Entries) {
                    if ($entry.FullName -notmatch $CpmaFrames) { continue }
                    $src = $entry.Open()
                    $dst = $out.CreateEntry($entry.FullName, [IO.Compression.CompressionLevel]::Optimal).Open()
                    try { $src.CopyTo($dst) } finally { $dst.Dispose(); $src.Dispose() }
                    $count++
                }
                $out.Dispose()
            } finally { $outStream.Dispose() }
            $pk3.Dispose()
            $zip.Dispose()
        } finally { $zipStream.Dispose() }

        if ($count -ne 160) { throw ("se esperaban 160 cuadros y hay {0}" -f $count) }
        Move-Item -LiteralPath $part -Destination $target -Force
        Write-Log 'Animaciones de CPMA listas.' 'Green'
    } catch {
        Write-Log ("No se pudieron preparar las animaciones de CPMA: {0}. El juego abre igual, sin ellas." -f $_.Exception.Message) 'Yellow'
        if (Test-Path -LiteralPath $part) { Remove-Item -LiteralPath $part -Force -ErrorAction SilentlyContinue }
    } finally {
        if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath -Force -ErrorAction SilentlyContinue }
    }
}

# ---------------------------------------------------------------------------
# Handel Gothic Ex letters the frag message. It is not ours to ship, so like the files above
# it comes from the player's own PC: the copy installed in Windows, or one already dropped in
# the game's fonts folder under any name. It is copied once to baseq3/fonts/handelgothicex.ttf;
# without it the game uses Quake Live's Handel Gothic.

$FragFontFamily = 'Handel Gothic Ex'
$FragFontFile = 'handelgothicex.ttf'

function Get-FontFamilyName([string]$Path) {
    try {
        Add-Type -AssemblyName System.Drawing
        $fonts = New-Object System.Drawing.Text.PrivateFontCollection
        try {
            $fonts.AddFontFile($Path)
            if ($fonts.Families.Count -gt 0) { return $fonts.Families[0].Name }
        } finally { $fonts.Dispose() }
    } catch { }
    return $null
}

function Initialize-FragMessageFont {
    $dir = Join-Path $env:APPDATA 'quakelive\baseq3\fonts'
    $target = Join-Path $dir $FragFontFile
    if (Test-Path -LiteralPath $target) { return }

    $found = $null
    foreach ($key in @('HKCU:\Software\Microsoft\Windows NT\CurrentVersion\Fonts', 'HKLM:\Software\Microsoft\Windows NT\CurrentVersion\Fonts')) {
        $props = Get-ItemProperty -LiteralPath $key -ErrorAction SilentlyContinue
        if (-not $props) { continue }
        foreach ($p in $props.PSObject.Properties) {
            if ($p.Name -notlike "$FragFontFamily*") { continue }
            $file = [string]$p.Value
            if (-not [IO.Path]::IsPathRooted($file)) { $file = Join-Path (Join-Path $env:WINDIR 'Fonts') $file }
            if ((Test-Path -LiteralPath $file) -and (Get-FontFamilyName $file) -eq $FragFontFamily) { $found = $file; break }
        }
        if ($found) { break }
    }
    if (-not $found -and (Test-Path -LiteralPath $dir)) {
        foreach ($f in @(Get-ChildItem -LiteralPath $dir -File -ErrorAction SilentlyContinue)) {
            if ($f.Extension -notin @('.ttf', '.otf')) { continue }
            if ((Get-FontFamilyName $f.FullName) -eq $FragFontFamily) { $found = $f.FullName; break }
        }
    }
    if (-not $found) { return }

    try {
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
        Copy-Item -LiteralPath $found -Destination $target -Force
        Write-Log 'Fuente del frag message lista: Handel Gothic Ex de este PC.' 'Green'
    } catch {
        Write-Log ("No se pudo preparar Handel Gothic Ex: {0}. El frag message usa la Handel Gothic de Quake Live." -f $_.Exception.Message) 'Yellow'
    }
}

function Start-Game([string]$Version, $ManifestArgs) {
    if (-not (Initialize-QuakeLiveData)) {
        Read-Host 'Presiona Enter para salir'
        exit 1
    }
    Initialize-CpmaAnimations
    Initialize-FragMessageFont
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
