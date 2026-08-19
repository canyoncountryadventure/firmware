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
        throw 'Build succeeded but no RAK4631 UF2 file was found.'
    }

    Write-Host "RAK4631 build complete."
    Write-Host "Flash file: $($uf2.FullName)"
}
finally {
    Pop-Location
}
