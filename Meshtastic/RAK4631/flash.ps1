$ErrorActionPreference = 'Stop'

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$pio = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\platformio.exe'

if (-not (Test-Path $pio)) {
    throw "PlatformIO not found at $pio"
}

Push-Location $repo
try {
    & $pio run -e rak4631
    if ($LASTEXITCODE -ne 0) {
        throw "RAK4631 build failed with exit code $LASTEXITCODE"
    }

    $uf2 = Get-ChildItem '.pio\build\rak4631\*.uf2' |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1

    if (-not $uf2) {
        throw 'No RAK4631 UF2 file was found after the build.'
    }

    $uf2Drives = @(
        Get-PSDrive -PSProvider FileSystem |
            Where-Object { Test-Path (Join-Path $_.Root 'INFO_UF2.TXT') }
    )

    if ($uf2Drives.Count -eq 0) {
        throw 'No UF2 bootloader drive found. Double-tap RESET on the RAK4631, then run this same command again.'
    }

    if ($uf2Drives.Count -gt 1) {
        throw 'More than one UF2 bootloader drive is mounted. Disconnect or exit bootloader mode on the other UF2 device, then run this command again.'
    }

    $drive = $uf2Drives[0].Root
    Copy-Item $uf2.FullName $drive -Force

    Write-Host "RAK4631 flash complete."
    Write-Host "Copied: $($uf2.FullName)"
    Write-Host "To: $drive"
}
finally {
    Pop-Location
}
