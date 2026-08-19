# Seeed XIAO nRF52840 + Wio-SX1262

**Status:** Production / hardware validated 2026-08-19  
**Branch:** `hobo-mx2001-mx2201-mx2203`  
**PlatformIO target:** `seeed_xiao_nrf52840_kit`

This is the production firmware path for the Seeed XIAO nRF52840 + Wio-SX1262 HOBO field node.

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

From the repository root, the easiest command is:

```powershell
& .\Meshtastic\SEEED-XIAO\build.ps1
```

The script uses the full PlatformIO executable path already used on this computer.

Manual equivalent:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e seeed_xiao_nrf52840_kit
```

## Where the flash file is

After a successful build, the UF2 is generated under:

```text
C:\Meshtastic\HOBO\firmware\.pio\build\seeed_xiao_nrf52840_kit\
```

The filename follows:

```text
firmware-seeed_xiao_nrf52840_kit-<version>.uf2
```

Use the newest `.uf2` in that directory. `<version>` changes with firmware version and commit.

## Flash — one command

With the Seeed connected by USB, run:

```powershell
& .\Meshtastic\SEEED-XIAO\flash.ps1
```

That command runs the PlatformIO upload target and prints the newest generated UF2 path when complete.

Manual equivalent:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e seeed_xiao_nrf52840_kit -t upload
```

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

On 2026-08-19, the same Seeed universal firmware was physically tested against MX2001, MX2201, and MX2203 without reflashing between logger models. It also saw all three loggers simultaneously and correctly maintained one HOBO BLE connection at a time.

## Source files used by this build

Shared universal HOBO implementation:

```text
src/modules/Telemetry/HOBOMX2001MX2201MX2203/
├── HOBOMX2001MX2201MX2203Telemetry.cpp
├── HOBOMX2001MX2201MX2203Telemetry.h
├── ONSETSDK.md
└── README.md
```

Seeed compatibility router:

```text
src/modules/Telemetry/HOBOMX2201MX2001/HOBOMX2201MX2001Telemetry.h
```

Seeed board definition:

```text
variants/nrf52840/seeed_xiao_nrf52840_kit/
```

Shared nRF52 BLE setup:

```text
src/platform/nrf52/NRF52Bluetooth.cpp
```

The compatibility router is intentional. Do not refactor it only for cosmetic reasons unless the resulting build is physically revalidated.
