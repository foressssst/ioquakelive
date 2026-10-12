param(
    [Parameter(Position=0)]
    [string]$TargetDir
)

$TargetDir = $TargetDir.Trim('"').TrimEnd('\')

if ($TargetDir -match '(?i)(.*baseq3)') {
    $baseq3 = $Matches[1]
} else {
    $baseq3 = Join-Path $TargetDir "baseq3"
}

if (-not (Test-Path $baseq3)) {
    New-Item -ItemType Directory -Path $baseq3 -Force | Out-Null
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptDir "..\..")).Path
$pak01Source = Join-Path $repoRoot "content\pak01"

if (-not (Test-Path $pak01Source)) {
    Write-Host "content\pak01 not found at $pak01Source; skipping pak01.pk3 creation"
    exit 0
}

$files = Get-ChildItem -Path $pak01Source -Recurse -File
if ($files.Count -eq 0) {
    Write-Host "No files found in $pak01Source; skipping pak01.pk3 creation"
    exit 0
}

$entries = New-Object System.Collections.Generic.List[object]
foreach ($file in $files) {
    $entries.Add(@($file.FullName, $file.FullName.Substring($pak01Source.Length + 1).Replace('\', '/')))
}

$pk3Path = Join-Path $baseq3 "pak01.pk3"

if (Test-Path $pk3Path) { Remove-Item $pk3Path -Force }

Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::Open($pk3Path, 'Create')

foreach ($entry in $entries) {
    [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
        $zip, $entry[0], $entry[1],
        [System.IO.Compression.CompressionLevel]::Optimal
    ) | Out-Null
}

$zip.Dispose()

Write-Host "pak01.pk3 created with $($entries.Count) file(s)"
