param(
	[ValidateSet('x64', 'ARM64')]
	[string]$Platform = 'x64',
	[string]$Msys = 'C:\msys64'
)

$ErrorActionPreference = 'Stop'

$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$dir = @{ x64 = 'win64'; ARM64 = 'winarm64' }[$Platform]
$arch = @{ x64 = 'x64'; ARM64 = 'x64_arm64' }[$Platform]
$out = Join-Path $root "code\libs\ffmpeg\$dir"

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -property installationPath
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
cmd /c "`"$vcvars`" $arch >nul && set" | ForEach-Object {
	if ($_ -match '^([^=]+)=(.*)$') {
		[Environment]::SetEnvironmentVariable($Matches[1], $Matches[2])
	}
}

$extra = if ($Platform -eq 'ARM64') { '--arch=aarch64 --target-os=win32 --enable-cross-compile --host-cc=cl' } else { '' }
$env:MSYSTEM = 'MSYS'
$env:MSYS2_PATH_TYPE = 'inherit'
$env:CHERE_INVOKING = '1'
$script = ($out -replace '\\', '/')
Push-Location $root
try {
	& (Join-Path $Msys 'usr\bin\bash.exe') -lc "sh code/tools/build-ffmpeg.sh '$script' --toolchain=msvc $extra"
	if ($LASTEXITCODE) { throw "FFmpeg build failed ($LASTEXITCODE)" }
} finally {
	Pop-Location
}
Write-Host "FFmpeg for $Platform in $out"
