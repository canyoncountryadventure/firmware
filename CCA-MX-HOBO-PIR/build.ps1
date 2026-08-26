$ErrorActionPreference = 'Stop'

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$pio = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\platformio.exe'
$environment = 'seeed_xiao_nrf52840_cca_mx_pir'

if (-not (Test-Path $pio)) {
    throw "PlatformIO not found at $pio"
}

Push-Location $repo
try {
    Write-Host 'Building CCA-MX-PIR 1.0.0 for Seeed XIAO nRF52840 + Wio-SX1262...'
    & $pio run -e $environment
    if ($LASTEXITCODE -ne 0) {
        throw "CCA build failed with exit code $LASTEXITCODE"
    }

    $uf2 = Get-ChildItem ".pio\build\$environment\*.uf2" |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1

    if (-not $uf2) {
        throw 'Build succeeded but no CCA Seeed UF2 file was found.'
    }

    Write-Host ''
    Write-Host 'CCA-MX-PIR build complete.'
    Write-Host "Flash file: $($uf2.FullName)"
}
finally {
    Pop-Location
}
