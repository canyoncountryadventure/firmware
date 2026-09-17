# RAK Soil Moisture + HOBO v1

Production field firmware for **RAK4631 + RAK19007** stations using a **DFRobot SEN0308 waterproof capacitive soil-moisture sensor** and optional **Onset HOBO MX-series BLE temperature/water-level loggers**.

**Branch:** `RAK-Soil-Moisture-HOBO-v1`

**Supported HOBO loggers:** MX2001, MX2201, MX2203.

This build combines the proven HOBO next-record telemetry path, SEN0308 analog soil moisture, direct-message diagnostics, raw-data preservation, non-destructive self-recovery, and nRF52840 watchdog protection for unattended stations.

## Hardware

### Soil sensor wiring — RAK19007

| SEN0308 lead | RAK19007 | Purpose |
|---|---|---|
| Red | `VDD` | 3.3 V power |
| Yellow | `AIN1` | Analog soil signal |
| Black | `GND` | Ground |
| Black | `GND` | Ground; both black leads are common |

RAK19007 `AIN1` maps to RAK4631 **A1 / P0.31 / AIN7**.

The RAK variant enables the WisBlock 3.3-V rail during startup, so the SEN0308 can be powered directly from `VDD`.

## SEN0308 field calibration

Calibration was measured on this exact SEN0308 + RAK4631/RAK19007 setup at 10-bit ADC resolution.

| Condition | Measured ADC10 |
|---|---:|
| Dry air | ~634 |
| Super-dry soil | ~580 |
| Slightly moist soil | ~294 |
| Wet/saturated soil | ~0 |
| Water | ~0 |

The field scale intentionally uses **soil endpoints**, not dry air:

- **0% moisture = ADC10 580**
- **100% moisture = ADC10 0**
- Higher ADC = drier
- Lower ADC = wetter

Conversion:

```text
moisture_percent = clamp((580 - ADC10) / 580 * 100, 0, 100)
```

The firmware averages **20 analog samples** separated by **10 ms** before calculating moisture.

Because saturated soil and open water both drive this sensor close to zero, the sensor should be treated as a soil-moisture indicator, not as a way to distinguish saturated soil from standing water.

## Soil telemetry

The node automatically samples and transmits soil moisture **once per hour** after the initial startup delay.

Each automatic soil reading produces:

1. A normal Meshtastic `TELEMETRY_APP` environment packet using the standard `soil_moisture` field, so ordinary Meshtastic telemetry consumers can display the calculated percentage.
2. A compact `PRIVATE_APP` raw packet so the original ADC measurement is retained for future recalibration.

Raw packet format, version 1:

| Byte | Meaning |
|---:|---|
| 0 | `S` |
| 1 | `M` |
| 2 | Packet version = `1` |
| 3 | Moisture percent, 0–100 |
| 4–5 | ADC10 raw value, little-endian |
| 6–7 | Soil telemetry sequence, little-endian |

Keeping the raw ADC value means the percentage calibration can be changed later without losing the original sensor response.

## HOBO automatic telemetry

The HOBO path is inherited from the field-proven RAK HOBO Safe firmware.

- Uses HOBO `STATUS` and the logger write pointer rather than a blind radio timer.
- Uses the proven `NEWREAD64` record path.
- Detects the logger's actual logging interval.
- Automatically transmits only after a real new HOBO record is written.
- Manual `READ` does **not** consume or advance the automatic write-pointer baseline.
- `LOCK` persists the intended logger assignment across reboot.
- Supports MX2001, MX2201 and MX2203.
- MX2201/MX2203 temperature is emitted as normal Meshtastic environmental temperature telemetry.
- MX2001 retains the existing combined water-level/temperature private-packet behavior.

## Direct-message commands

Send commands as a direct Meshtastic text message to the node. Commands are case-insensitive; a leading `/` is optional where supported.

### Soil commands

| Command | What it does |
|---|---|
| `SOIL` | Takes a fresh soil reading and replies with moisture % and raw ADC10. |
| `SOIL READ` | Same as `SOIL`. DM-only; does not create an automatic telemetry event. |
| `SOIL STATUS` | Shows the last reading, automatic interval, input pin and calibration endpoints. |
| `SOIL TX` | Takes a fresh reading, broadcasts standard soil telemetry + raw packet, and reports TX status by DM. |
| `SOIL CAL` | Shows the active dry/wet calibration and sensor direction. |
| `SOIL HELP` | Lists soil commands. |

### HOBO commands

| Command | What it does |
|---|---|
| `LOGGER` | Shows HOBO model, MAC, BLE RSSI, logging interval, target and lock state. |
| `READ` | Requests a fresh HOBO measurement without consuming the automatic record pointer. |
| `LOCK` | Saves the currently identified HOBO as this station's logger. |
| `UNLOCK` | Clears the saved logger assignment and resumes discovery. |
| `AUTO` | Confirms pointer-gated next-record automatic HOBO operation. |

### Node / recovery commands

| Command | What it does |
|---|---|
| `HELP` | Shows the existing HOBO/recovery command summary. Use `SOIL HELP` for soil commands. |
| `STATUS` / `HEALTH` | Compact node and self-recovery health. |
| `POWER` / `BATTERY` | Battery voltage, percentage, battery-present and charging state. |
| `BLE` | BLE scanner/link state, disconnected age and restart count. |
| `WATCHDOG` | Shows watchdog state, owner and timeout. |
| `STATS` | Recovery counts and boot/reset information. |
| `NODES` | Meshtastic NodeDB count. |
| `UPTIME` | Node uptime. |
| `VERSION` | Self-recovery firmware/platform identity. |
| `PING` / `WAKE` | End-to-end DM/liveness check. |
| `SCAN` | Refreshes BLE scanning when disconnected. |
| `RECONNECT` | Rebuilds the disconnected BLE scanner/link. |
| `RECOVER` / `REBOOT` | Replies, then performs a safe non-destructive reboot. |

## Watchdog and unattended recovery

This branch retains the **nRF52840 hardware watchdog** from `RAK-HOBO-Safe-v1`.

- If the Meshtastic core already owns the watchdog, the self-recovery module leaves ownership unchanged.
- Otherwise it arms the on-chip watchdog for **15 minutes**.
- The watchdog remains active during sleep.
- The self-recovery supervisor feeds it during healthy operation.
- BLE scanning is periodically refreshed while disconnected.
- A BLE stack that remains disconnected/stale for six hours triggers a safe reboot.
- `RECOVER` / `REBOOT` provides the same non-destructive reboot path remotely.

Routine recovery must preserve:

- Meshtastic node identity
- LoRa/channel settings and PSKs
- NodeDB
- saved HOBO logger assignment

Factory erase is **not** part of normal recovery.

## Recommended bench / field validation

After flashing:

```text
PING
WATCHDOG
POWER
SOIL READ
SOIL STATUS
SOIL TX
LOGGER
READ
LOCK
LOGGER
AUTO
BLE
STATUS
```

For soil validation, compare `SOIL READ` against known conditions. The original bench points were approximately 580 in super-dry soil, 294 in slightly moist soil, and 0 in saturated soil.

For HOBO validation, lock the correct logger, reboot, confirm `LOGGER` restores the assignment, then wait through at least one real logger interval and verify automatic telemetry occurs only after the next logger record.

## Build target

```text
rak4631
```

The branch uses the same validated Meshtastic baseline as the RAK HOBO Safe build. The GitHub Actions workflow builds the RAK4631 release artifacts and publishes a permanent UF2/OTA copy for this branch.

Use the normal UF2 or BLE DFU package for routine updates. Do not use factory-erase firmware for ordinary upgrades.
