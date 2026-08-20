# Meshtastic HOBO Project

This folder is the user-facing project structure for the HOBO → Meshtastic firmware.

## Branches to use

**Production branch:**

```text
hobo-mx2001-mx2201-mx2203
```

**Frozen hardware-validated snapshot:**

```text
hobo-universal-validated-2026-08-19
```

Use the production branch for normal builds and deployments. The frozen branch exists as a rollback/reference point for the exact validated universal implementation.

## What the firmware does automatically

Once connected to a supported HOBO, the radio reads the logger's own `STATUS` response to learn:

- logging interval in seconds;
- current write pointer.

It then waits for the HOBO write pointer to advance. A confirmed new logger record triggers one fresh `NEWREAD64` measurement and one Meshtastic transmission. Automatic telemetry is therefore synchronized to the HOBO's actual record creation, not to an unrelated radio timer.

If pointer tracking fails, automatic telemetry pauses until `STATUS` recovers rather than transmitting on a guessed schedule.

Final RAK4631 bench validation with an MX2201 configured at 20 seconds measured automatic packet cadences of 19.848 s and 19.879 s, with about 202 ms from detected logger record to telemetry queueing.

## Direct Meshtastic commands

Send these directly to the field radio:

```text
LOGGER
READ
LOCK
UNLOCK
```

- `LOGGER` — model, logger MAC, BLE RSSI, detected logging interval, lock state and target.
- `READ` — immediate fresh measurement; does not consume or reset automatic interval tracking.
- `LOCK` — persistently assign the radio to the currently identified HOBO MAC.
- `UNLOCK` — clear the assignment and resume general HOBO discovery.

For field deployment, use `LOGGER` to verify the physical logger first, then `LOCK` at the site.

## Project layout

```text
Meshtastic/
├── README.md
├── SEEED-XIAO/
│   ├── README.md
│   ├── build.ps1
│   └── flash.ps1
├── RAK4631/
│   ├── README.md
│   ├── build.ps1
│   └── flash.ps1
├── SHARED-HOBO/
│   └── README.md
└── ARCHIVE/
    └── README.md
```

## Supported radio targets

### Seeed XIAO nRF52840 + Wio-SX1262

Guide: [`SEEED-XIAO/README.md`](SEEED-XIAO/README.md)

PlatformIO target:

```text
seeed_xiao_nrf52840_kit
```

### RAK4631 / RAK19003

Guide: [`RAK4631/README.md`](RAK4631/README.md)

PlatformIO target:

```text
rak4631
```

## Supported HOBO loggers

| Logger | Automatic telemetry | Direct `READ` |
|---|---|---|
| MX2001 | Water level + temperature | Water level + temperature |
| MX2201 | Temperature | Temperature |
| MX2203 | Temperature | Temperature |

The same universal HOBO implementation is used on both supported radios.

See [`SHARED-HOBO/README.md`](SHARED-HOBO/README.md) for the exact automatic interval behavior, command semantics, protocol details, and validation notes.

## Local Windows repository

```text
C:\Meshtastic-HOBO\firmware
```

Generated PlatformIO build files are under:

```text
C:\Meshtastic-HOBO\firmware\.pio\build\
```

Sync production with:

```powershell
cd C:\Meshtastic-HOBO\firmware
git fetch origin
git switch hobo-mx2001-mx2201-mx2203
git pull --ff-only origin hobo-mx2001-mx2201-mx2203
```

## Old branches

Old model-specific, discovery, integration, recovery, and raw-debug branches are retained only for rollback/protocol history. Do not use them for normal field deployment. See [`ARCHIVE/README.md`](ARCHIVE/README.md).