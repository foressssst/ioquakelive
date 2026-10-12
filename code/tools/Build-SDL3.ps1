param(
	[string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$source = Join-Path $root 'code\SDL3'
$build = Join-Path $root 'build\SDL3-arm64'
$out = Join-Path $root 'code\libs\winarm64'

$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake) {
	$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
	$vs = & $vswhere -latest -products * -property installationPath
	$cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
}

& $cmake -S $source -B $build -A ARM64 `
	-DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST_LIBRARY=OFF `
	"-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$(if ($Configuration -eq 'Debug') { 'Debug' })"
if ($LASTEXITCODE) { throw "cmake configure failed ($LASTEXITCODE)" }

& $cmake --build $build --config $Configuration --parallel
if ($LASTEXITCODE) { throw "cmake build failed ($LASTEXITCODE)" }

New-Item -ItemType Directory -Force -Path $out | Out-Null
Copy-Item (Join-Path $build "$Configuration\SDL3.dll") $out -Force
Copy-Item (Join-Path $build "$Configuration\SDL3.lib") $out -Force
Write-Host "SDL3 for ARM64 in $out"
