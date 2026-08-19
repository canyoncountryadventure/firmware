# HOBO MX2001 + MX2201 + MX2203 → Meshtastic

**Recommended production branch:** `hobo-mx2001-mx2201-mx2203`  
**Target:** Seeed XIAO nRF52840 + Wio-SX1262  
**Status:** Hardware-validated universal on-demand reader

This is the preferred Seeed firmware for current HOBO deployments. One firmware image supports three Onset HOBO logger models:

| Logger | Live data returned by `READ` |
|---|---|
| **MX2001** | Water level + temperature |
| **MX2201** | Temperature |
| **MX2203** | Temperature |

## Production status

Use this branch for new Seeed HOBO nodes unless there is a specific reason to deploy a model-specific recovery branch.

```text
hobo-mx2001-mx2201-mx2203   ← recommended universal build
```

The model-specific branches remain available as clean recovery/reference builds:

```text
hobo-mx2001
hobo-mx2201
hobo-mx2203
hobo-mx2201-mx2001
```

Historical `*-integration`, discovery, and test branches are archived and should not be used for new deployments. See [`archive/README.md`](archive/README.md).

## Hardware validation — 2026-08-19

The same flashed Seeed XIAO firmware was tested against all three physical logger models without reflashing between models.

Test sequence:

1. MX2203 exposed while MX2201 and MX2001 were covered — identified as `MX2203` and completed live reads.
2. MX2201 exposed while the other two were covered — identified as `MX2201` and completed live reads.
3. MX2001 exposed while the other two were covered — identified as `MX2001` and completed live reads.
4. All three loggers were then exposed simultaneously. The node discovered candidates sequentially and connected to one valid logger at a time; in this test it ultimately connected to the MX2201.

This validates the universal firmware with all three physical logger models. The BLE bridge is intentionally **single-logger-at-a-time**: it supports any of the three models, but it does not maintain simultaneous BLE connections to all three.

Transient BLE service-discovery failures were observed during logger switching, followed by successful rediscovery/reconnection. Successful reads after those retries confirm the model decoders were operating correctly.

## Deployment rule

For the current firmware, think in terms of:

```text
1 monitoring site = 1 Meshtastic radio
```

A site can use MX2001, MX2201, or MX2203 without changing firmware. If multiple HOBOs are colocated at one site, a future sequential multi-logger polling mode can be added; the current production build intentionally connects to only one logger at a time.

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

The older `HOBOMX2201MX2001` directory remains only as a tiny compatibility router because the existing Meshtastic module hook uses that include path. It contains no logger implementation on this branch. The production implementation is entirely in `HOBOMX2001MX2201MX2203`.

## Model identification

The bridge first sends the shared Onset `INIT` command and then the shared `NEWREAD64` live-read command. The live response identifies the model:

| Model | Response signature |
|---|---|
| MX2201 | `01 01 07 04 04 00 04 04 ...` |
| MX2203 | `01 01 0B 04 04 00 04 04 ...` |
| MX2001 | known two-fragment MX2001 response |

The logger MAC address is learned dynamically; no physical logger MAC is hard-coded.

## MX2203 OnsetSDK conversion

The MX2203 conversion was recovered from `OnsetSDK.dll` inside the HOBOconnect Android APK and validated against the physical logger export:

```text
C = raw × 175.72 / 16384 - 46.85
F = C × 9/5 + 32
```

The permanent APK/reverse-engineering record is in [`src/modules/Telemetry/HOBOMX2001MX2201MX2203/ONSETSDK.md`](src/modules/Telemetry/HOBOMX2001MX2201MX2203/ONSETSDK.md).

## Build and flash

```powershell
cd C:\Meshtastic\HOBO\firmware
git fetch origin
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

## Repository policy

- New Seeed HOBO deployments: use `hobo-mx2001-mx2201-mx2203`.
- Model-specific branches: keep as recovery/reference builds.
- Archived test branches: preserve for history, do not deploy.
- Do not replace the OnsetSDK conversion with an empirical regression; the APK-derived formula is the authoritative MX2203 conversion in this project.
- Do not merge experimental multi-logger polling into this branch until it has been physically validated.
