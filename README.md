# HOBO MX2001 + MX2201 + MX2203 → Meshtastic

**Production branch:** `hobo-mx2001-mx2201-mx2203`  
**Target:** Seeed XIAO nRF52840 + Wio-SX1262  
**Status:** Hardware-validated three-model universal on-demand reader

One firmware image supports three Onset HOBO logger models:

| Logger | Live data returned by `READ` |
|---|---|
| **MX2001** | Water level + temperature |
| **MX2201** | Temperature |
| **MX2203** | Temperature |

## Hardware validation — 2026-08-19

The same flashed Seeed XIAO firmware was tested against all three physical logger models without reflashing between models.

Test sequence:

1. MX2203 exposed while MX2201 and MX2001 were covered — identified as `MX2203` and completed live reads.
2. MX2201 exposed while the other two were covered — identified as `MX2201` and completed live reads.
3. MX2001 exposed while the other two were covered — identified as `MX2001` and completed live reads.
4. All three loggers were then exposed simultaneously. The node discovered candidates sequentially and connected to one valid logger at a time; in this test it ultimately connected to the MX2201.

This validates the universal firmware with all three physical logger models. The BLE bridge is intentionally **single-logger-at-a-time**: it supports any of the three models, but it does not maintain simultaneous BLE connections to all three.

Transient BLE service-discovery failures were observed during logger switching, followed by successful rediscovery/reconnection. The successful model reads after those retries show this was transient connection behavior rather than a model-decoder failure.

## How it works

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
   MX2001 / MX2201 / MX2203
            │
            │ live NEWREAD64
            ▼
       fresh measurement
            │
            ▼
reliable Meshtastic direct reply
```

There is **no periodic HOBO polling** in this universal build. The node connects, identifies the logger from the live BLE protocol, and waits for a direct `READ` command.

## Command

Send a direct Meshtastic text message to the Seeed:

```text
READ
```

`/READ`, lowercase, and mixed case are accepted.

Example replies:

```text
MX2001
Level: 1.04 ft
Temp: 78.9 F
```

```text
MX2201
Temp: 70.5 F
```

```text
MX2203
Temp: 72.38 F / 22.43 C
```

## Production source

```text
src/modules/Telemetry/HOBOMX2001MX2201MX2203/
├── HOBOMX2001MX2201MX2203Telemetry.cpp
├── HOBOMX2001MX2201MX2203Telemetry.h
├── ONSETSDK.md
└── README.md
```

The older `HOBOMX2201MX2001` directory remains only as a tiny compatibility router because `Modules.cpp` already uses that include path. It contains no logger implementation on this branch.

## Model identification

The bridge first sends the shared Onset `INIT` command and then the shared `NEWREAD64` live-read command. The live response identifies the model:

| Model | Response signature |
|---|---|
| MX2201 | `01 01 07 04 04 00 04 04 ...` |
| MX2203 | `01 01 0B 04 04 00 04 04 ...` |
| MX2001 | known two-fragment MX2001 response |

The logger MAC address is learned dynamically; no physical logger MAC is hard-coded.

## MX2203 official conversion

The MX2203 conversion was recovered from `OnsetSDK.dll` inside the HOBOconnect Android APK and validated against the physical logger export:

```text
C = raw × 175.72 / 16384 - 46.85
F = C × 9/5 + 32
```

See [`src/modules/Telemetry/HOBOMX2001MX2201MX2203/ONSETSDK.md`](src/modules/Telemetry/HOBOMX2001MX2201MX2203/ONSETSDK.md).

## Build and flash

```powershell
cd C:\Meshtastic\HOBO\firmware
git fetch origin
git switch --track origin/hobo-mx2001-mx2201-mx2203
```

If the local branch already exists:

```powershell
git switch hobo-mx2001-mx2201-mx2203
git pull origin hobo-mx2001-mx2201-mx2203
```

Build:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e seeed_xiao_nrf52840_kit
```

Flash:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e seeed_xiao_nrf52840_kit -t upload
```

Optional serial monitor:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor -b 115200
```

## Other production branches

```text
hobo-mx2001
hobo-mx2201
hobo-mx2201-mx2001
hobo-mx2203
hobo-mx2001-mx2201-mx2203   ← universal three-model build
```

Historical discovery/test branches are retained only for rollback and protocol history and should not be used for new deployments.
