# RAK Water Distance + HOBO v1

Production combined firmware for **RAK4631 + RAK19007** field nodes that must do both:

- ultrasonic water-level / stage monitoring, and
- BLE collection from supported HOBO MX loggers.

**Branch:** `RAK-Water-Distance-HOBO-v1`

This branch is built from the current **RAK Water Distance v1** firmware and then adds the proven HOBO reader/recovery stack. It replaces the older `distance-hobo-safe` combined build.

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
STATUS
RAW
VERIFY
INTERVAL 1H
CAL STAGE 1.42FT
CAL STATUS
TELEMETRY NOW
```

Replace `1.42FT` with the independently measured stage at the site.

After calibration, completely remove power, reconnect it, then verify:

```text
STATUS
CAL STATUS
READ
```

To discard a bench/test calibration before deployment:

```text
CAL RESET CONFIRM
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

## Useful water commands

```text
HELP
STATUS
CHECK
SENSOR
SENSOR A01NYUB
SENSOR A02YYUW
SENSOR SEN0590
SENSOR AUTO
READ
RAW
VERIFY
INTERVAL 15MIN
INTERVAL 1H
CAL STAGE 1.42FT
CAL STATUS
CAL LOCK
CAL UNLOCK CONFIRM
CAL RESET CONFIRM
TELEMETRY NOW
RESET WATER CONFIRM
```

Recovery commands such as `POWER`, `WATCHDOG`, `REBOOT`, and `RECOVER` are supplied by the HOBO self-recovery layer in this combined build.

## Build and flashing

Meshtastic base remains pinned to the validated **2.7.26** baseline.

Use normal RAK4631 UF2 or BLE DFU packages for routine updates. **Do not use factory-erase images for normal upgrades.**

A successful compile is required, but field deployment still requires real sensor, HOBO BLE, calibration-persistence, telemetry, and recovery testing on the actual assembled node.
