# RAK4631 HOBO Mesh Firmware

**Branch:** `hobo-mx2001-mx2201-mx2203-rak4631`  
**Target:** RAK4631 / WisBlock  
**Purpose:** normal Meshtastic mesh radio + direct HOBO BLE reader  
**Custom PIR/trail-counter code:** **not included**

This branch is the RAK4631 HOBO deployment build. One RAK4631 can connect to one nearby Onset HOBO logger and automatically place fresh measurements onto the Meshtastic mesh as standard environmental telemetry.

Supported loggers:

| Logger | Data |
|---|---|
| **MX2001** | temperature + water level/stage |
| **MX2201** | temperature |
| **MX2203** | temperature |

## Automatic operation

Automatic mesh transmission is enabled on this branch.

```text
RAK4631 boots
    │
    ├─ normal Meshtastic radio remains active
    │
    └─ BLE central discovers one HOBO
             │
             ├─ startup live read
             │      └─ broadcast TELEMETRY_APP
             │
             └─ automatic live read every 60 minutes
                    └─ broadcast TELEMETRY_APP
```

The automatic interval defaults to **60 minutes**. A failed automatic read retries after **60 seconds**. The interval can be changed at build time with `CCA_HOBO_AUTO_READ_INTERVAL_MS`.

The outgoing packet is standard Meshtastic `TELEMETRY_APP` environmental telemetry, so another Meshtastic node such as the Heltec Home gateway can receive it without a custom text parser. Temperature is sent in Celsius. MX2001 stage is sent in the environmental `distance` field in millimetres.

A manual direct-message `READ` command is still supported and returns a direct text reply. Manual reads do not create a duplicate automatic telemetry broadcast.

## Station policy

For the CCA deployment:

- **Hidden Valley:** automatic HOBO read/transmit station. This RAK behavior is appropriate for it.
- **Home:** automatic local HOBO reading is handled by the Heltec Home gateway.
- **Fishlake:** **not automatic on the remote radio**. Fishlake is trigger/poll driven by the Heltec branch, which sends a `READ` request when it wants a Fishlake measurement.

## No PIR

This branch does not add the custom PIR/trail-counter firmware used by the trail-sensor builds. The branch is intentionally focused on:

1. Meshtastic mesh operation.
2. HOBO MX2001/MX2201/MX2203 BLE reading.
3. Automatic standard telemetry transmission.
4. Direct `READ` support for diagnostics or a trigger-driven deployment.

## Manual READ

Send a direct Meshtastic text message to the RAK node:

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

## Source

```text
src/modules/Telemetry/HOBOMX2001MX2201MX2203/
├── HOBOMX2001MX2201MX2203Telemetry.cpp       shared HOBO protocol/decoder
├── HOBOMX2001MX2201MX2203Telemetry.h
├── HOBOMX2001MX2201MX2203TelemetryRAK.cpp    RAK auto-read + mesh broadcast policy
├── HOBOMX2001MX2201MX2203TelemetryRAK.h
├── ONSETSDK.md
└── README.md
```

The RAK wrapper reuses the hardware-proven universal HOBO protocol implementation and adds the RAK deployment behavior without adding PIR logic.

## Build

```powershell
cd C:\Meshtastic\HOBO\firmware
git fetch origin
git switch hobo-mx2001-mx2201-mx2203-rak4631
git pull --ff-only origin hobo-mx2001-mx2201-mx2203-rak4631
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e rak4631
```

Do not flash unless the build finishes with `SUCCESS`.

## Flash

The RAK4631 normally uses UF2 drag-and-drop in bootloader mode. After a successful build, use the generated RAK4631 UF2 under:

```text
.pio\build\rak4631\
```

Double-reset the RAK4631 to expose the bootloader drive, then copy the generated UF2 to it.

## Runtime logs to expect

At startup:

```text
RAK HOBO mesh: automatic reads enabled ...; PIR disabled
```

When the timer fires:

```text
RAK HOBO mesh: automatic live read triggered
```

After a successful mesh transmission:

```text
RAK HOBO mesh: auto broadcast model=MX2201 temp=... C
```

## Model identification

The shared bridge sends the Onset `INIT` and `NEWREAD64` commands and identifies the logger from its live BLE response. Logger MAC addresses are learned dynamically; no physical HOBO MAC is hard-coded.

The MX2203 conversion is the OnsetSDK-derived formula documented in [`src/modules/Telemetry/HOBOMX2001MX2201MX2203/ONSETSDK.md`](src/modules/Telemetry/HOBOMX2001MX2201MX2203/ONSETSDK.md).
