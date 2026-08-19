$ErrorActionPreference = 'Stop'

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$pio = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\platformio.exe'

if (-not (Test-Path $pio)) {
    throw "PlatformIO not found at $pio"
}

Push-Location $repo
try {
    & $pio run -e seeed_xiao_nrf52840_kit -t upload
    if ($LASTEXITCODE -ne 0) {
        throw "Seeed upload failed with exit code $LASTEXITCODE"
    }

    $uf2 = Get-ChildItem '.pio\build\seeed_xiao_nrf52840_kit\*.uf2' |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1

    Write-Host 'Seeed flash complete.'
    if ($uf2) {
        Write-Host "Generated UF2: $($uf2.FullName)"
    }
}
finally {
    Pop-Location
}
