> **Field-Recovery v2:** hardened SX1262 recovery, dual nRF52 watchdog channels, missed RX/TX IRQ polling, guarded AGC calibration, radio power-cycle recovery, flash-safe reboot, 12-hour burn-in reboot, and increased nRF52/BLE task stacks. Sensor behavior from Seeed-Water-Distance-HOBO-v1 is retained.

# Seeed Water Distance + HOBO v1

Production combined firmware for **Seeed XIAO nRF52840 + Wio-SX1262** field nodes that must do both:

- ultrasonic water-level / stage monitoring, and
- BLE collection from supported HOBO MX loggers.

**Branch:** `Seeed-Water-Distance-HOBO-v2`

This branch is built from the current **Seeed Water Distance v1** firmware and then adds the proven HOBO reader/recovery stack. It replaces the older combined distance/HOBO builds.

## Scope

Water side:

- DFRobot SEN0313 / A01NYUB — default
- DFRobot SEN0311 / A02YYUW
- DFRobot SEN0590
- permanent water mode
- fresh-frame distance reads
- stage calibration with automatic lock
- configurable report interval
- redundant A/B persistent config with sequence, CRC32, exact-size checking, truncation before overwrite, and read-back verification
- water telemetry on Meshtastic channel 0

HOBO side:

- MX2001
- MX2201
- MX2203
- BLE scanning/collection using the existing universal HOBO reader
- HOBO field check and self-recovery helpers

Trail-counter logic is intentionally excluded.

## Recovery ownership

The combined build uses **one recovery supervisor**: the HOBO self-recovery module. The standalone distance recovery supervisor is not instantiated in this combined build. This avoids duplicate watchdog/reboot command handlers while retaining watchdog, BLE recovery, remote recovery commands, and field diagnostics.

Water configuration persistence remains handled by the Water Distance v1 module.

## Water field setup

Use a second Meshtastic node to DM the field node:

```text
WATER STATUS
WATER RAW
WATER VERIFY
WATER INTERVAL 1H
WATER CAL STAGE 1.42FT
WATER CAL STATUS
WATER TELEMETRY NOW
```

Replace `1.42FT` with the independently measured stage at the site. The explicit `WATER` prefix is recommended in this combined build so water commands are unmistakable.

After calibration, completely remove power, reconnect it, then verify:

```text
WATER STATUS
WATER CAL STATUS
WATER READ
```

To discard a bench/test calibration before deployment:

```text
WATER CAL RESET CONFIRM
```

This clears only water calibration; sensor selection and reporting interval are preserved.

## Seeed A01NYUB wiring

```text
A01NYUB red   -> 3V3
A01NYUB black -> GND
A01NYUB green -> XIAO D7 / UART RX
A01NYUB blue  -> leave floating for stabilized output
```

Wio-SX1262 uses D4/D5. Water UART remains on D6/D7 with sensor TX connected to D7.

## Water persistence

Water settings are stored in redundant A/B flash records. The inactive slot is rewound and truncated before each write, then the complete record is re-opened and verified before it becomes active. The previous valid slot remains the fallback until verification succeeds.

Normal battery loss, solar shutdown, reboot, watchdog reset, or ordinary non-destructive firmware updates do not intentionally erase water calibration, interval, Meshtastic identity, channels, keys, or NodeDB.

## A01NYUB power note

A01NYUB ranges continuously while powered, so its blue LED keeps blinking even when water telemetry is hourly. Actual sensor sleep requires hardware power gating with a load switch or high-side MOSFET.

## Useful DM commands

### Water side

| Command | What it does |
|---|---|
| `WATER HELP` | Shows the water command summary. |
| `WATER STATUS` | Reports water sensor, interval, calibration, and readiness. |
| `WATER CHECK` | Performs a fresh water-sensor check. |
| `WATER SENSOR` | Shows the selected distance-sensor driver. |
| `WATER SENSOR A01NYUB` | Selects the SEN0313/A01NYUB sensor. |
| `WATER SENSOR A02YYUW` | Selects the SEN0311/A02YYUW sensor. |
| `WATER SENSOR SEN0590` | Selects the SEN0590 sensor. |
| `WATER SENSOR AUTO` | Automatically tries supported distance-sensor drivers. |
| `WATER READ` | Takes a fresh water-distance reading and reports stage if calibrated. |
| `WATER RAW` | Returns raw distance without stage conversion. |
| `WATER VERIFY` | Takes multiple fresh readings and reports median/range/spread. |
| `WATER INTERVAL 1H` | Saves a 1-hour automatic water-report interval. |
| `WATER CAL STAGE 1.42FT` | Calibrates water stage from an independent field measurement and locks it. |
| `WATER CAL STATUS` | Shows saved water calibration and lock state. |
| `WATER CAL UNLOCK CONFIRM` | Unlocks the calibration without erasing it. |
| `WATER CAL RESET CONFIRM` | Clears only the water calibration. |
| `WATER TELEMETRY NOW` | Immediately sends a fresh water telemetry packet. |
| `WATER RESET WATER CONFIRM` | Resets the water subsystem to defaults without factory-resetting Meshtastic. |

### HOBO / recovery side

| Command | What it does |
|---|---|
| `LOGGER` | Shows HOBO model, MAC, BLE RSSI, logging interval, and lock state. |
| `READ` | Requests an immediate fresh HOBO reading without consuming the automatic pointer. |
| `LOCK` | Saves the currently identified HOBO as this station's logger. |
| `UNLOCK` | Clears the saved HOBO assignment and resumes discovery. |
| `BLE` | Reports BLE link/scanning state and recovery information. |
| `AUTO` | Reports the HOBO automatic-record / pointer-gated state. |
| `SCAN` | Refreshes BLE scanning when disconnected. |
| `RECONNECT` | Rebuilds the disconnected BLE scanner/link. |
| `POWER` | Reports battery/power status. |
| `WATCHDOG` | Reports watchdog state/ownership. |
| `PING` | Quick end-to-end DM/liveness test. |
| `REBOOT` | Performs a safe non-destructive reboot after replying. |
| `RECOVER` | Alias for the safe recovery reboot. |

Commands are case-insensitive. A leading `/` is optional where supported.

## Build and flashing

Meshtastic base remains pinned to the validated **2.7.26** baseline.

Use normal Seeed UF2 or BLE DFU packages for routine updates. **Do not use factory-erase images for normal upgrades.**

A successful compile is required, but field deployment still requires real sensor, HOBO BLE, calibration-persistence, telemetry, and recovery testing on the actual assembled node.
