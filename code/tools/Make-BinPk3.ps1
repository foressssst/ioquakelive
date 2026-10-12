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
    Write-Warning "baseq3 directory not found: $baseq3"
    exit 0
}

$mutex = New-Object System.Threading.Mutex($false, "Global\IobinPk3Build")
$mutex.WaitOne() | Out-Null
try {
    Push-Location $baseq3

    $dlls = Get-ChildItem -Filter "*.dll" -File
    if ($dlls.Count -eq 0) {
        Write-Host "No DLLs found in $baseq3; skipping iobin pk3 creation"
        Pop-Location
        exit 0
    }

    $pk3Name = "iobin.pk3"
    $zipName = "iobin.zip"

    if (Test-Path $pk3Name) { Remove-Item $pk3Name -Force }
    if (Test-Path $zipName) { Remove-Item $zipName -Force }

    Compress-Archive -Path $dlls.FullName -DestinationPath $zipName
    Rename-Item $zipName $pk3Name -Force

    Write-Host "$pk3Name created with $($dlls.Count) DLL(s)"

    Pop-Location
} finally {
    $mutex.ReleaseMutex()
}
