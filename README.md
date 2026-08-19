# HOBO MX2203 → Meshtastic Bridge

**Final production branch:** `hobo-mx2203`  
**Logger:** Onset HOBO MX2203  
**Radio:** Seeed XIAO nRF52840 + Wio-SX1262  
**Meshtastic region:** US915

This branch contains the finalized MX2203 integration that lets a Meshtastic node read the live temperature from a HOBO MX2203 over Bluetooth and return it on demand over the mesh.

## What it does

```text
Remote Meshtastic node / phone
            │
            │ direct message: READ
            ▼
       Meshtastic mesh
            │
            ▼
Seeed XIAO nRF52840 + Wio-SX1262
            │
            │ BLE central
            ▼
       HOBO MX2203
            │
            │ NEWREAD64
            ▼
     live temperature
            │
            ▼
reliable Meshtastic direct reply
```

There is **no automatic periodic HOBO polling** in this final build. The bridge connects to the logger, waits idle, and performs a fresh logger read only when it receives a direct `READ` command.

## Use

Send a direct Meshtastic text message to the Seeed node:

```text
READ
```

`/READ`, lowercase, and mixed case are also accepted.

Example reply:

```text
MX2203
Temp: 72.38 F / 22.43 C
```

The response is generated from a fresh BLE read and is sent back to the requesting node as a reliable direct Meshtastic message.

## Production source

The MX2203 implementation has its own folder:

```text
src/modules/Telemetry/HOBOMX2203/
├── HOBOMX2203Telemetry.cpp
├── HOBOMX2203Telemetry.h
├── ONSETSDK.md
└── README.md
```

See **[`src/modules/Telemetry/HOBOMX2203/README.md`](src/modules/Telemetry/HOBOMX2203/README.md)** for the complete protocol, runtime behavior, validation results, and flashing notes.

See **[`src/modules/Telemetry/HOBOMX2203/ONSETSDK.md`](src/modules/Telemetry/HOBOMX2203/ONSETSDK.md)** for the permanent record of the HOBOconnect APK reverse engineering and the official Onset temperature conversion.

## Confirmed MX2203 BLE protocol

| Item | Value |
|---|---|
| Onset manufacturer prefix | `C5 00` |
| MX2203 advertisement discriminator | `... 01 03 22 02 ...` |
| HOBO service UUID | `CFCBE6BC-CC83-49AC-4146-4EED4F6EE165` |
| Command characteristic | `CFCBE6BC-CC83-49AC-4146-4EED4F6FE165` |
| INIT | `01 01 04 05 1C 01 00` |
| Live read (`NEWREAD64`) | `01 01 08 04 04 00 00 00 00 00 00` |
| MX2203 response prefix | `01 01 0B 04 04 00 04 04` |
| Temperature raw value | next 4 bytes, big-endian |

The logger MAC address is **not hard-coded**. The bridge discovers an MX2203 from its Onset manufacturer/model advertisement.

## Official OnsetSDK temperature conversion

The conversion used here is not a fitted approximation.

On **2026-08-19**, the HOBOconnect Android APKs were unpacked and `OnsetSDK.dll` was recovered from the application's .NET Android assembly store. The relevant Onset code maps:

```text
MX2201 / MX2202 → TempSensor32 → 12-bit
MX2203 / MX2204 / MX2205 → TempSensor2F → 14-bit
```

For MX2203, OnsetSDK uses:

```text
C = raw × 175.72 / 16384 - 46.85
F = C × 9/5 + 32
```

The APKs used in that investigation were:

```text
HOBOconnect-base.apk
HOBOconnect-arm64.apk
```

That reverse-engineering result is permanently documented in [`ONSETSDK.md`](src/modules/Telemetry/HOBOMX2203/ONSETSDK.md) so the conversion does not need to be rediscovered later.

## Hardware validation

The MX2203 BLE response and OnsetSDK conversion were validated on physical hardware.

A hot-to-cold water-bath run produced serial raw readings that aligned with the HOBO-exported temperature data. Example matches:

| Raw | OnsetSDK result | HOBO export |
|---:|---:|---:|
| 7489 | 92.25 F | 92.25 F |
| 7177 | 86.22 F | 86.22 F |
| 6460 | 72.38 F | 72.38 F |
| 5089 | 45.91 F | 45.91 F |

## Runtime behavior

1. Meshtastic boots normally.
2. The Seeed scans passively for an MX2203.
3. It connects to the HOBO BLE service.
4. It sends `INIT` once.
5. It waits idle.
6. A direct Meshtastic `READ` message triggers one fresh `NEWREAD64` request.
7. The four-byte raw temperature is decoded with the OnsetSDK `TempSensor2F` formula.
8. The Seeed sends the temperature directly back to the requester.
9. The bridge returns to idle.
10. If the HOBO disconnects, scanning resumes automatically.

## Flash this final branch

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

Serial monitoring is not required for normal field operation.

## Branch history

- `hobo-mx2201-mx2001` remains the stable, proven combined MX2201/MX2001 implementation.
- `mx2203-discovery-test` retains the discovery/calibration history.
- **`hobo-mx2203` is the final production MX2203 branch.**

The MX2203 final branch is intentionally kept separate so future MX2203 work can be changed without risking the proven MX2201/MX2001 implementation.
