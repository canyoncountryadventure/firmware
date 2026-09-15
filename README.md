# Meshtastic Water-Distance Firmware — Seeed

Production-oriented **water-level / stage firmware** for:

- Seeed XIAO nRF52840
- Wio-SX1262 LoRa radio
- DFRobot SEN0313 / A01NYUB as the default sensor
- DFRobot SEN0311 / A02YYUW and SEN0590 also supported

This branch is **water-only**. Trail-counter logic is intentionally excluded and will be maintained separately.

## Branch

`water-distance-seeed`

Base Meshtastic version remains pinned to the project's validated 2.7.26 baseline. Do not upgrade the Meshtastic base casually.

## Field behavior

On a fresh configuration the node defaults to:

- A01NYUB sensor
- 1-hour report interval
- uncalibrated stage
- automatic water telemetry on Meshtastic channel 0

The node does not require a `MODE WATER` command. Water mode is permanent in this branch.

## Field DM workflow

Use a second Meshtastic node to DM the sensor node. Custom water commands are intentionally ignored when they originate from the sensor node itself.

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

`CAL STAGE` takes multiple fresh ultrasonic readings, uses the median, calculates the stage reference, writes the calibration to persistent storage, verifies the saved record, and locks calibration automatically.

## Calibration safety

```text
CAL STATUS
CAL LOCK
CAL UNLOCK
CAL UNLOCK CONFIRM
CAL RESET
CAL RESET CONFIRM
```

Changing a calibrated sensor is blocked while calibration is locked.

`CAL RESET CONFIRM` clears only the water calibration. It preserves the selected sensor and report interval.

## Water-subsystem reset

```text
RESET WATER
RESET WATER CONFIRM
```

The confirmed reset returns the water subsystem to A01NYUB, 1-hour reporting, and no calibration. It does **not** erase Meshtastic node identity, channels, keys, or NodeDB.

## Persistence design

Water configuration is stored in redundant A/B records with:

- monotonically increasing sequence numbers
- CRC32 validation
- exact record-size validation
- write/read-back verification
- inactive-slot overwrite before the active slot is changed

On nRF52, Adafruit LittleFS `FILE_O_WRITE` opens an existing file at EOF. This branch explicitly seeks to offset 0 and truncates the inactive config slot before writing, preventing multiple config records from being appended into one slot.

A failed save is rejected and the prior valid active slot remains authoritative.

## Sensor reads

UART sensors use fresh-frame reads. Queued stale A01NYUB/A02YYUW frames are discarded before a requested or scheduled reading so the reported distance represents the current target.

`VERIFY` takes multiple fresh readings and reports the median and spread.

Three consecutive sensor errors trigger a sensor-interface reinitialization attempt.

## A01NYUB LED / power behavior

The A01NYUB performs continuous ranging whenever it is powered, so its blue activity LED continues blinking even when this firmware only records/transmits hourly. The current hardware wiring does not power-gate the sensor.

Reducing the sensor LED/ranging to the reporting interval requires a hardware load switch or high-side MOSFET so firmware can remove sensor power between measurements.

## Recovery

This branch retains the field self-recovery foundation, including:

- nRF52840 watchdog protection
- remote recovery/reboot support
- battery/device telemetry
- low-voltage / solar recovery behavior
- persistent Meshtastic identity and radio configuration
- non-destructive normal firmware updates

Routine recovery must never factory-reset the node.

## Seeed wiring — A01NYUB

```text
A01NYUB red   -> 3V3
A01NYUB black -> GND
A01NYUB green -> XIAO D7 / UART RX
A01NYUB blue  -> leave floating for stabilized output
```

Wio-SX1262 already occupies D4/D5. The water UART is kept on D6/D7, with the sensor TX connected to D7.

## Useful commands

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

Recovery commands such as `POWER`, `WATCHDOG`, `REBOOT`, and `RECOVER` are provided by the retained recovery subsystem.

## Safe flashing

Use the normal board-specific firmware/UF2 or BLE DFU package. Do not use factory-erase images for normal upgrades. Normal updates are expected to preserve Meshtastic configuration and saved water settings.
