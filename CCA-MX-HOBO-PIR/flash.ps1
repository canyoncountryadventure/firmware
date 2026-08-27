$ErrorActionPreference = 'Stop'

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$pio = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\platformio.exe'
$environment = 'seeed_xiao_nrf52840_cca_mx_pir'

if (-not (Test-Path $pio)) {
    throw "PlatformIO not found at $pio"
}

Push-Location $repo
try {
    Write-Host 'Building and flashing CCA-MX-PIR 1.0.7...'
    & $pio run -e $environment -t upload
    if ($LASTEXITCODE -ne 0) {
        throw "CCA upload failed with exit code $LASTEXITCODE"
    }

    $uf2 = Get-ChildItem ".pio\build\$environment\*.uf2" |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1

    Write-Host ''
    Write-Host 'CCA-MX-PIR 1.0.7 flash complete.'
    if ($uf2) {
        Write-Host "Generated UF2: $($uf2.FullName)"
    }
}
finally {
    Pop-Location
}
