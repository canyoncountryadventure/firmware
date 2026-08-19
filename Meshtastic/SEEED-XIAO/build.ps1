$ErrorActionPreference = 'Stop'

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$pio = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\platformio.exe'

if (-not (Test-Path $pio)) {
    throw "PlatformIO not found at $pio"
}

Push-Location $repo
try {
    & $pio run -e seeed_xiao_nrf52840_kit
    if ($LASTEXITCODE -ne 0) {
        throw "Seeed build failed with exit code $LASTEXITCODE"
    }

    $uf2 = Get-ChildItem '.pio\build\seeed_xiao_nrf52840_kit\*.uf2' |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1

    if (-not $uf2) {
        throw 'Build succeeded but no Seeed UF2 file was found.'
    }

    Write-Host "Seeed build complete."
    Write-Host "Flash file: $($uf2.FullName)"
}
finally {
    Pop-Location
}
