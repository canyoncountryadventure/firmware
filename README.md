# RAK Water Distance v1

Production water-level / stage firmware for **RAK4631 + RAK19007** Meshtastic field nodes.

**Branch:** `RAK-Water-Distance-v1`

**Status:** Builds successfully on the RAK4631 target and carries the same water-only persistence, calibration, telemetry, and recovery design as the validated Seeed build. Full field hardware validation on RAK remains separate from the Seeed validation.

This branch is intentionally **water-only**. Trail-counter firmware belongs in a separate branch.

## Hardware

Default sensor:

- DFRobot SEN0313 / A01NYUB ultrasonic distance sensor

Also supported:

- DFRobot SEN0311 / A02YYUW
- DFRobot SEN0590

RAK UART mapping used by the water sensor code:

```text
UART1 RX -> logical pin 15
UART1 TX -> logical pin 16
```

For a streaming A01NYUB, sensor TX goes to the node UART RX.

## Default behavior

Fresh water configuration defaults to:

- A01NYUB
- 1-hour report interval
- no stage calibration
- automatic water telemetry on Meshtastic channel 0
- permanent water mode; no `MODE WATER` command is needed

Base Meshtastic remains pinned to the project's validated **2.7.26** baseline.

## Field installation workflow

Use a second Meshtastic node to DM the sensor node.

```text
STATUS
RAW
VERIFY
INTERVAL 1H
CAL STAGE 1.42FT
CAL STATUS
TELEMETRY NOW
```

Replace `1.42FT` with the independently measured water stage at the site.

`CAL STAGE` takes multiple fresh ultrasonic samples, uses the median, calculates the stage reference, writes the result to persistent flash, verifies the saved record, and locks calibration automatically.

After calibration, hard power-cycle the node and verify:

```text
STATUS
CAL STATUS
READ
```

For a test calibration that should be discarded before deployment:

```text
CAL RESET CONFIRM
```

That clears only calibration and preserves the selected sensor and interval.

## Persistent configuration

Water settings use redundant **A/B flash records** with:

- sequence numbers
- CRC32 validation
- exact record-size validation
- write/read-back verification
- inactive-slot overwrite before the active slot changes

On nRF52 Adafruit LittleFS, `FILE_O_WRITE` opens an existing file at EOF. This firmware explicitly seeks to offset 0 and truncates the inactive slot before writing. The previous valid slot remains untouched until the new record verifies.

Normal power loss, reboot, watchdog reset, or non-destructive firmware update does not intentionally erase calibration, interval, channels, keys, identity, or NodeDB.

## Calibration safety commands

```text
CAL STATUS
CAL LOCK
CAL UNLOCK
CAL UNLOCK CONFIRM
CAL RESET
CAL RESET CONFIRM
```

Sensor changes are blocked while calibration is locked.

Full water-subsystem reset:

```text
RESET WATER
RESET WATER CONFIRM
```

The confirmed reset returns the water subsystem to A01NYUB, 1-hour reporting, and no calibration. It does **not** factory-reset Meshtastic.

## Sensor reads and recovery

UART distance reads discard queued stale frames before each requested or scheduled sample. `VERIFY` takes multiple fresh readings and reports median/spread. Three consecutive sensor read failures trigger sensor-interface reinitialization.

The inherited field-recovery foundation retains:

- nRF52840 watchdog protection
- remote reboot/recovery support
- battery/device telemetry
- low-voltage and solar recovery behavior
- persistent Meshtastic identity/configuration
- non-destructive routine recovery

## A01NYUB LED / power note

The A01NYUB ranges continuously whenever powered, so its blue LED continues blinking even if firmware reports only once per hour. Reducing sensor activity to the reporting interval requires hardware power gating with a load switch or high-side MOSFET.

## Useful DM commands

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

Recovery commands include `POWER`, `WATCHDOG`, `REBOOT`, and `RECOVER`.

## Safe flashing

Use the normal RAK4631 UF2 or BLE DFU package for routine updates. **Do not use factory-erase images for normal upgrades.**
