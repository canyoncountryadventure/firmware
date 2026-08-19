# RAK4631 Universal HOBO Support — Validation Record

**Canonical branch:** `hobo-mx2001-mx2201-mx2203`  
**Hardware:** RAK4631 / RAK19003  
**Status:** Hardware validated on 2026-08-19

This file preserves the RAK4631 validation record. RAK support has been folded into the canonical universal branch. The former `hobo-mx2001-mx2201-mx2203-rak4631` branch is historical only.

## Validated logger paths

- **HOBO MX2001** — water level + temperature
- **HOBO MX2201** — temperature
- **HOBO MX2203** — temperature
- direct Meshtastic text command `READ`

The RAK nRF52 Bluetooth layer reserves one BLE peripheral link for the Meshtastic phone connection and one BLE central link for the HOBO logger.

## 2026-08-19 hardware result

The RAK universal build physically connected to and completed reads from all three logger models.

An apparent MX2201 108.4 F error was investigated and determined to be a valid reading from a second nearby MX2201 located near a hot attic. It was not a parser or conversion fault.

A follow-up raw NEWREAD64 capture on another MX2201 produced:

```text
Raw: 1726
Water Temp: 80.95 F
Water Temp: 27.20 C
```

This confirmed the RAK MX2201 live-response byte parsing and conversion path.

## Production architecture

The Seeed implementation remains unchanged. On RAK4631, the small file:

```text
src/modules/Telemetry/HOBOMX2001MX2201MX2203/HOBOMX2001MX2201MX2203TelemetryRAK.cpp
```

loads the shared universal implementation under the real RAK4631 board configuration. The legacy RAK `MX2001Diagnostic` module hook is aliased to the universal module class, and the old MX2001-only RAK implementation is removed on the canonical branch.

## Build

```powershell
cd C:\Meshtastic\HOBO\firmware
git fetch origin
git switch hobo-mx2001-mx2201-mx2203
git pull --ff-only origin hobo-mx2001-mx2201-mx2203
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e rak4631
```

## Flash

Double-tap RESET on the RAK4631 so the UF2 bootloader drive is mounted, then run this single PowerShell command:

```powershell
$uf2=(Get-ChildItem ".pio\build\rak4631\*.uf2" | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName; $drive=(Get-PSDrive -PSProvider FileSystem | Where-Object { Test-Path "$($_.Root)INFO_UF2.TXT" } | Select-Object -First 1).Root; if (-not $drive) { throw "RAK bootloader drive not found - double-tap RESET first" }; Copy-Item $uf2 $drive -Force; Write-Host "Flashed $uf2 to $drive"
```

## Expected direct replies

```text
MX2203
Temp: 72.38 F / 22.43 C
```

```text
MX2201
Temp: 70.5 F
```

```text
MX2001
Level: 1.04 ft
Temp: 78.9 F
```

No additional three-model protocol validation is required for this RAK port unless the shared reader behavior is changed.
