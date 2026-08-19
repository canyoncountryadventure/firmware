# RAK4631 / RAK19003

**Status:** Production / hardware validated 2026-08-19  
**Branch:** `hobo-mx2001-mx2201-mx2203`  
**PlatformIO target:** `rak4631`

This is the production firmware path for the RAK4631 mounted on a RAK19003 base board.

## Supported loggers

The same firmware image supports:

- MX2001 — water level + temperature
- MX2201 — temperature
- MX2203 — temperature

Send a direct Meshtastic text message:

```text
READ
```

The node performs a fresh BLE read and replies to the requesting Meshtastic node.

## Sync the production branch

```powershell
cd C:\Meshtastic\HOBO\firmware
git fetch origin
git switch hobo-mx2001-mx2201-mx2203
git pull --ff-only origin hobo-mx2001-mx2201-mx2203
```

## Build

From the repository root, use:

```powershell
& .\Meshtastic\RAK4631\build.ps1
```

Manual equivalent:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e rak4631
```

## Where the flash file is

After a successful build, the UF2 is generated under:

```text
C:\Meshtastic\HOBO\firmware\.pio\build\rak4631\
```

The filename follows:

```text
firmware-rak4631-<version>.uf2
```

Example from the 2026-08-19 validation work:

```text
firmware-rak4631-2.7.26.fd44453.uf2
```

The version/commit suffix changes. Use the newest `.uf2` in the `rak4631` build directory.

## Flash — one command

1. Connect the RAK by USB.
2. Double-tap RESET so the UF2 bootloader drive appears in Windows.
3. Run one command from the repository root:

```powershell
& .\Meshtastic\RAK4631\flash.ps1
```

The script:

- builds the current `rak4631` production firmware;
- finds the newest generated RAK UF2;
- finds the mounted UF2 bootloader drive;
- refuses to continue if zero or multiple UF2 drives are mounted;
- copies the firmware to the bootloader drive.

This replaces the old multi-step "find filename, find drive letter, then copy" process.

## Serial monitor

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor -b 115200
```

## Expected replies

MX2001:

```text
MX2001
Level: 1.04 ft
Temp: 78.9 F
```

MX2201:

```text
MX2201
Temp: 70.5 F
```

MX2203:

```text
MX2203
Temp: 72.38 F / 22.43 C
```

## Hardware validation

On 2026-08-19, the RAK4631 universal build physically completed reads from MX2001, MX2201, and MX2203. The apparent 108.4 F MX2201 problem was a false alarm: the RAK had connected to a second nearby MX2201 in a hot attic area where approximately 108 F was legitimate. A dedicated raw-read check on the other MX2201 returned raw 1726 = 80.95 F, confirming the RAK NEWREAD64 decode and conversion path.

No further model-by-model validation is required for the production code that was merged from that tested RAK build.

## Source files used by this build

Shared universal HOBO implementation:

```text
src/modules/Telemetry/HOBOMX2001MX2201MX2203/
├── HOBOMX2001MX2201MX2203Telemetry.cpp
├── HOBOMX2001MX2201MX2203Telemetry.h
├── HOBOMX2001MX2201MX2203TelemetryRAK.cpp
├── ONSETSDK.md
└── README.md
```

RAK compatibility module hook:

```text
src/modules/Telemetry/MX2001Diagnostic.h
```

RAK board definition:

```text
variants/nrf52840/rak4631/
```

Shared nRF52 BLE setup:

```text
src/platform/nrf52/NRF52Bluetooth.cpp
```

The RAK adapter intentionally reuses the hardware-proven universal HOBO implementation rather than maintaining a second protocol implementation.
