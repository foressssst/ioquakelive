param(
    [Parameter(Position=0)]
    [string]$TargetDir
)

# Strip trailing quotes/backslashes that MSBuild's "$(TargetDir)" can produce
$TargetDir = $TargetDir.Trim('"').TrimEnd('\')

# Find the baseq3 directory from the target path
if ($TargetDir -match '(?i)(.*baseq3)') {
    $baseq3 = $Matches[1]
} else {
    $baseq3 = Join-Path $TargetDir "baseq3"
}

if (-not (Test-Path $baseq3)) {
    New-Item -ItemType Directory -Path $baseq3 -Force | Out-Null
}

# content/pak01 is relative to the repo root (two levels up from misc/msvc)
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

# [ioquakelive] Files that ship in pak01 but stay out of the repo, because ioquakelive has no
# licence to publish them: CPMA 1.53's animation frames and Handel Gothic Ex. They come from
# assets\pak01 next to the repo when that folder exists; a path in both places stops the build.
$extraSource = Join-Path (Split-Path -Parent $repoRoot) "assets\pak01"
$extraCount = 0
if (Test-Path $extraSource) {
    $seen = @{}
    foreach ($entry in $entries) { $seen[$entry[1].ToLowerInvariant()] = $true }
    foreach ($file in Get-ChildItem -Path $extraSource -Recurse -File) {
        $relativePath = $file.FullName.Substring($extraSource.Length + 1).Replace('\', '/')
        if ($seen.ContainsKey($relativePath.ToLowerInvariant())) {
            Write-Host "error: $relativePath is both in content\pak01 and in $extraSource"
            exit 1
        }
        $entries.Add(@($file.FullName, $relativePath))
        $extraCount++
    }
}

$pk3Path = Join-Path $baseq3 "pak01.pk3"

if (Test-Path $pk3Path) { Remove-Item $pk3Path -Force }

# Use .NET ZipFile for correct forward-slash paths inside the archive
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::Open($pk3Path, 'Create')

foreach ($entry in $entries) {
    [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
        $zip, $entry[0], $entry[1],
        [System.IO.Compression.CompressionLevel]::Optimal
    ) | Out-Null
}

$zip.Dispose()

Write-Host "pak01.pk3 created with $($entries.Count) file(s), $extraCount of them from $extraSource"
