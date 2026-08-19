# HOBO MX2203 → Meshtastic Bridge

**Status:** Final field build  
**Branch:** `hobo-mx2203`  
**Target:** Seeed XIAO nRF52840 + Wio-SX1262 / Meshtastic US915

This module turns a Meshtastic Seeed node into an on-demand Bluetooth bridge for an **Onset HOBO MX2203** temperature logger.

## What it does

```text
Phone / remote Meshtastic node
          │
          │ direct message: READ
          ▼
Meshtastic mesh → Seeed XIAO nRF52840
                         │
                         │ BLE central
                         ▼
                    HOBO MX2203
                         │
                         │ live NEWREAD64
                         ▼
                 current temperature
                         │
                         ▼
       reliable Meshtastic direct reply
```

There is **no automatic periodic logger polling**. The bridge stays connected and idle until it receives a direct `READ` command.

## User command

Send a **direct Meshtastic message** to the Seeed node:

```text
READ
```

`/READ`, lowercase, and mixed case are also accepted.

Example reply:

```text
MX2203
Temp: 72.38 F / 22.43 C
```

The reply is sent back to the requesting node as a reliable direct Meshtastic text message.

## Final MX2203 BLE protocol

| Item | Value |
|---|---|
| Onset manufacturer prefix | `C5 00` |
| MX2203 advertisement discriminator | `... 01 03 22 02 ...` |
| HOBO service UUID | `CFCBE6BC-CC83-49AC-4146-4EED4F6EE165` |
| Command characteristic | `CFCBE6BC-CC83-49AC-4146-4EED4F6FE165` |
| INIT | `01 01 04 05 1C 01 00` |
| Live read (`NEWREAD64`) | `01 01 08 04 04 00 00 00 00 00 00` |
| MX2203 live-response prefix | `01 01 0B 04 04 00 04 04` |
| Temperature raw | next 4 bytes, big-endian |

The bridge identifies the logger from the MX2203 manufacturer discriminator, so it does not hard-code one logger MAC address.

## Official Onset temperature conversion

The final conversion is **not an empirical regression**. It was recovered from `OnsetSDK.dll` inside the HOBOconnect Android application.

HOBOconnect maps:

- MX2201 / MX2202 → `TempSensor32` → 12-bit
- **MX2203 / MX2204 / MX2205 → `TempSensor2F` → 14-bit**

For the MX2203, OnsetSDK uses:

```text
C = raw × 175.72 / 16384 - 46.85
F = C × 9/5 + 32
```

See [`ONSETSDK.md`](./ONSETSDK.md) for the permanent APK/reverse-engineering record.

## Hardware validation

The MX2203 protocol and conversion were validated on physical hardware on **2026-08-19**.

A hot-to-cold water-bath run produced 40 consecutive serial raw values that aligned with the HOBO-exported temperature data. The OnsetSDK equation reproduces the HOBO values to the logger's displayed precision across the run.

Examples:

| Raw | OnsetSDK result | HOBO export |
|---:|---:|---:|
| 7489 | 92.25 F | 92.25 F |
| 7177 | 86.22 F | 86.22 F |
| 6460 | 72.38 F | 72.38 F |
| 5089 | 45.91 F | 45.91 F |

## BLE architecture

The Seeed remains a normal Meshtastic BLE peripheral for the phone while also acting as a BLE central for the HOBO logger.

The branch inherits the proven nRF52 dual-role configuration from the production MX2201/MX2001 branch:

```cpp
Bluefruit.configCentralBandwidth(BANDWIDTH_LOW);
Bluefruit.begin(1, 1);
```

The MX2203 bridge uses passive scanning and then maintains the HOBO connection while waiting for `READ`.

## Flashing

From the local firmware checkout:

```powershell
cd C:\Meshtastic\HOBO\firmware
git fetch origin
git switch hobo-mx2203
git pull origin hobo-mx2203
```

Build:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e seeed_xiao_nrf52840_kit
```

Upload:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e seeed_xiao_nrf52840_kit -t upload
```

Optional serial monitor:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor -b 115200
```

Serial monitoring is not required for normal use.

## Runtime behavior

- Finds MX2203 by Onset manufacturer/model advertisement.
- Connects to the HOBO BLE command service.
- Sends `INIT` once after connection.
- Waits idle.
- Accepts `READ` only when sent directly to this Meshtastic node.
- Sends one fresh `NEWREAD64` command.
- Decodes the 14-bit `TempSensor2F` value using the OnsetSDK equation.
- Replies directly to the requester.
- Returns to idle.
- If the logger disconnects, scanning resumes automatically.

## Source files

```text
src/modules/Telemetry/HOBOMX2203/
├── HOBOMX2203Telemetry.cpp
├── HOBOMX2203Telemetry.h
├── ONSETSDK.md
└── README.md
```

This folder is the production MX2203 implementation. Calibration/discovery test modules are intentionally not part of the final branch.
