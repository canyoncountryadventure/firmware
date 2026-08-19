# HOBO MX2001 + MX2201 + MX2203 → Meshtastic

**Recommended production branch:** `hobo-mx2001-mx2201-mx2203`  
**Hardware targets:** Seeed XIAO nRF52840 + Wio-SX1262; RAK4631 / RAK19003  
**Status:** Hardware-validated universal on-demand reader on both platforms

This is the canonical firmware for current HOBO deployments. One protocol implementation supports three Onset HOBO logger models:

| Logger | Live data returned by `READ` |
|---|---|
| **MX2001** | Water level + temperature |
| **MX2201** | Temperature |
| **MX2203** | Temperature |

## Production status

Use this branch for new Seeed or RAK4631 HOBO nodes unless there is a specific reason to deploy a model-specific recovery branch.

```text
hobo-mx2001-mx2201-mx2203   ← canonical universal build
```

Model-specific branches remain available as clean recovery/reference builds:

```text
hobo-mx2001
hobo-mx2201
hobo-mx2203
hobo-mx2201-mx2001
```

Historical integration, discovery, RAK validation, and raw-debug branches are retained only for rollback/protocol history. See [`archive/README.md`](archive/README.md).

## Hardware validation — 2026-08-19

### Seeed XIAO nRF52840 + Wio-SX1262

The same flashed Seeed firmware was tested against all three physical logger models without reflashing between models:

1. MX2203 — identified correctly and completed live reads.
2. MX2201 — identified correctly and completed live reads.
3. MX2001 — identified correctly and completed live reads.
4. All three exposed simultaneously — the node discovered valid candidates and maintained one logger BLE connection at a time.

### RAK4631 / RAK19003

The RAK4631 port was then built and flashed from the same universal implementation and physically tested against all three logger models:

1. MX2201 — connected and completed live reads.
2. MX2203 — connected and completed live reads.
3. MX2001 — connected and completed live reads.

An apparent MX2201 high-temperature fault during testing was confirmed to be a valid reading from a second nearby MX2201 located near a hot attic, not a decoding error. A follow-up raw NEWREAD64 capture on another MX2201 returned raw `1726`, decoded to `80.95 F`, confirming the RAK MX2201 raw parsing/conversion path.

The canonical branch now contains the exact RAK adapter that passed this hardware test. The Seeed universal implementation was not rewritten during the RAK merge.

## Deployment rule

For the current firmware:

```text
1 monitoring site = 1 Meshtastic radio = 1 nearby HOBO connection at a time
```

The logger can be MX2001, MX2201, or MX2203 without changing firmware. Discovery is dynamic; no physical logger MAC is hard-coded in the production universal reader.

If multiple HOBOs are simultaneously within BLE range, the node may connect to whichever valid candidate it discovers first. This is acceptable for the intended field deployment where one logger is near each radio. Multi-logger polling or MAC binding can be added later if a deployment requires it.

## How it works

```text
Remote Meshtastic node / phone
            │
            │ direct message: READ
            ▼
       Meshtastic mesh
            │
            ▼
Seeed XIAO/Wio or RAK4631/RAK19003
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

Send a direct Meshtastic text message to the HOBO bridge node:

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
├── HOBOMX2001MX2201MX2203TelemetryRAK.cpp
├── ONSETSDK.md
└── README.md
```

The main universal `.cpp` remains the hardware-proven Seeed implementation. The small `TelemetryRAK.cpp` compile adapter reuses that exact implementation under the real RAK4631 board configuration. The existing Meshtastic RAK module hook is routed to the universal class through `MX2001Diagnostic.h`; the old MX2001-only RAK implementation is removed on this branch.

The nRF52 Bluetooth layer configures both supported targets for one BLE peripheral connection (Meshtastic phone) and one BLE central connection (HOBO logger).

## Model identification

The bridge first sends the shared Onset `INIT` command and then the shared `NEWREAD64` live-read command. The live response identifies the model:

| Model | Response signature |
|---|---|
| MX2201 | `01 01 07 04 04 00 04 04 ...` |
| MX2203 | `01 01 0B 04 04 00 04 04 ...` |
| MX2001 | known two-fragment MX2001 response |

## Temperature decoding

### MX2201

The universal reader preserves the previously hardware-proven MX2201 conversion used by the combined reader. The HOBOconnect `OnsetSDK.dll` record also documents the exact `TempSensor32` relationship; do not casually change the production conversion without revalidation.

### MX2203

The MX2203 conversion was recovered from `OnsetSDK.dll` inside the HOBOconnect Android APK and validated against the physical logger export:

```text
C = raw × 175.72 / 16384 - 46.85
F = C × 9/5 + 32
```

The permanent APK/reverse-engineering record is in [`src/modules/Telemetry/HOBOMX2001MX2201MX2203/ONSETSDK.md`](src/modules/Telemetry/HOBOMX2001MX2201MX2203/ONSETSDK.md).

## Build

```powershell
cd C:\Meshtastic\HOBO\firmware
git fetch origin
git switch hobo-mx2001-mx2201-mx2203
git pull --ff-only origin hobo-mx2001-mx2201-mx2203
```

### Seeed

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e seeed_xiao_nrf52840_kit
```

Flash Seeed:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e seeed_xiao_nrf52840_kit -t upload
```

### RAK4631

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e rak4631
```

After the RAK bootloader drive is mounted by double-tapping RESET, flash the newest UF2 with one PowerShell command:

```powershell
$uf2=(Get-ChildItem ".pio\build\rak4631\*.uf2" | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName; $drive=(Get-PSDrive -PSProvider FileSystem | Where-Object { Test-Path "$($_.Root)INFO_UF2.TXT" } | Select-Object -First 1).Root; if (-not $drive) { throw "RAK bootloader drive not found - double-tap RESET first" }; Copy-Item $uf2 $drive -Force; Write-Host "Flashed $uf2 to $drive"
```

Optional serial monitor for either board:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor -b 115200
```

## Repository policy

- New Seeed or RAK4631 HOBO deployments: use `hobo-mx2001-mx2201-mx2203`.
- Model-specific branches: keep as recovery/reference builds.
- RAK validation/raw-debug branches: historical only after the 2026-08-19 merge.
- Archived test branches: preserve for history, do not deploy.
- Do not replace the APK-derived MX2203 formula with an empirical regression.
- Do not merge experimental multi-logger polling into this branch until it has been physically validated.
