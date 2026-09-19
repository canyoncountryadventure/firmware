> **Field-Recovery v2:** hardened SX1262 recovery, dual nRF52 watchdog channels, missed RX/TX IRQ polling, guarded AGC calibration, radio rail power-cycle recovery, flash-safe reboot, 12-hour burn-in reboot, and increased nRF52/BLE task stacks. Sensor behavior from RAK-Water-Distance-HOBO-v1 is retained.

# RAK Water Distance + HOBO v2

Production combined firmware for **RAK4631 + RAK19007** field nodes that must do both:

- ultrasonic water-level / stage monitoring, and
- BLE collection from supported HOBO MX loggers.

**Branch:** `RAK-Water-Distance-HOBO-v2`

This branch is built from the current **RAK Water Distance v1** firmware and then adds the proven HOBO reader/recovery stack. It replaces the older combined distance/HOBO builds.

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
- HOBO v2 diagnostics plus automatic BLE link recovery owned by the HOBO telemetry state machine

Trail-counter logic is intentionally excluded.

## Recovery ownership

The combined build uses **one node-recovery command/diagnostic module**: the HOBO self-recovery module. The standalone distance recovery supervisor is not instantiated. BLE scanner/link lifecycle remains owned by the HOBO telemetry state machine, which handles connection timeouts and repeated STATUS/NEWREAD failures. This avoids competing BLE owners and duplicate reboot command handlers while retaining the independent Field-Recovery v2 watchdog escalation path.

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

Replace `1.42FT` with the independently measured stage at the site. The `WATER` prefix is **required** in this combined build. Bare `READ`, `STATUS`, and other unprefixed commands are reserved for the HOBO/recovery side so the two command systems cannot collide.

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

## RAK UART mapping

```text
UART1 RX -> logical pin 15
UART1 TX -> logical pin 16
```

For the streaming A01NYUB, sensor TX goes to the node UART RX.

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
| `BLE` | Reports central-link count, scanner state, and HOBO state-machine ownership. |
| `AUTO` | Reports the HOBO automatic-record / pointer-gated state. |
| `SCAN` | Legacy diagnostic command; v2 reports that BLE recovery is automatic and does not manipulate the scanner. |
| `RECONNECT` | Legacy diagnostic command; v2 does not force a link rebuild because the HOBO telemetry state machine owns scanner/link lifecycle. |
| `POWER` | Reports battery/power status. |
| `WATCHDOG` | Reports the 90-second core watchdog, independent field-health channel, and sleep/halt behavior. |
| `PING` | Quick end-to-end DM/liveness test. |
| `REBOOT` | Performs a safe non-destructive reboot after replying. |
| `RECOVER` | Alias for the safe recovery reboot. |

Commands are case-insensitive. A leading `/` is optional where supported.

## Build and flashing

Meshtastic base remains pinned to the validated **2.7.26** baseline.

Use normal RAK4631 UF2 or BLE DFU packages for routine updates. **Do not use factory-erase images for normal upgrades.**

A successful compile is required, but field deployment still requires real sensor, HOBO BLE, calibration-persistence, telemetry, and recovery testing on the actual assembled node.


## Command-reply retry protection

Normal water and HOBO command replies are sent without requesting mesh ACK/retry. This prevents one diagnostic reply from being retransmitted repeatedly when the controller-side ACK path is weak or unavailable. The firmware-update `DFU` path remains separately protected by its reliable update workflow.

The water command parser accepts only explicit `WATER ...` commands in this combined build. Bare `READ`, `STATUS`, `SENSOR`, and similar text is ignored by the water module.
