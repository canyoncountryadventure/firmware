> **Field-Recovery v2:** hardened SX1262 recovery, dual nRF52 watchdog channels, missed RX/TX IRQ polling, guarded AGC calibration, radio power-cycle recovery, flash-safe reboot, 12-hour burn-in reboot, and increased nRF52/BLE task stacks. Sensor behavior from Seeed-Water-Distance-v1 is retained.

# Seeed Water Distance v2

Production water-level / stage firmware for **Seeed XIAO nRF52840 + Wio-SX1262** Meshtastic field nodes.

**Branch:** `Seeed-Water-Distance-v2`

**Status:** Seeed hardware validated through fresh sensor reads, calibration, persistent A/B config saves, hard power-cycle restore, interval changes, mesh telemetry, and remote DM commands.

This branch is intentionally **water-only**. Trail-counter firmware belongs in a separate branch.

## Hardware

Default sensor:

- DFRobot SEN0313 / A01NYUB ultrasonic distance sensor

Also supported:

- DFRobot SEN0311 / A02YYUW
- DFRobot SEN0590

Seeed A01NYUB wiring:

```text
A01NYUB red   -> 3V3
A01NYUB black -> GND
A01NYUB green -> XIAO D7 / UART RX
A01NYUB blue  -> leave floating for stabilized output
```

Wio-SX1262 uses D4/D5. The water UART remains on D6/D7, with sensor TX connected to D7.

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

Sensor changes are blocked while calibration is locked. `RESET WATER CONFIRM` resets only the water subsystem to A01NYUB, 1-hour reporting, and no calibration; it does **not** factory-reset Meshtastic.

## Sensor reads and recovery

UART distance reads discard queued stale frames before each requested or scheduled sample. `VERIFY` takes multiple fresh readings and reports median/spread. Three consecutive sensor read failures trigger sensor-interface reinitialization.

The Field-Recovery v2 foundation adds:

- a 90-second nRF52840 main-loop watchdog plus an independent field-health watchdog channel
- watchdog operation during CPU sleep/halt
- missed RX/TX IRQ polling and guarded SX1262 calibration
- full SX1262 state recovery with radio power cycling where supported
- flash-safe remote reboot/recovery through the centralized nRF52 reset path
- a 12-hour preventive whole-node reboot during v2 burn-in
- battery/device telemetry, low-voltage/solar recovery, and persistent Meshtastic identity/configuration

## A01NYUB LED / power note

The A01NYUB ranges continuously whenever powered, so its blue LED continues blinking even if firmware reports only once per hour. Reducing sensor activity to the reporting interval requires hardware power gating with a load switch or high-side MOSFET.

## Useful DM commands

| Command | What it does |
|---|---|
| `HELP` | Shows the water command summary. |
| `STATUS` | Reports water-sensor state, calibration state, interval, and readiness. |
| `CHECK` | Performs a fresh sensor check and reports whether the station is ready. |
| `SENSOR` | Shows the currently selected distance-sensor driver. |
| `SENSOR A01NYUB` | Selects the SEN0313/A01NYUB UART sensor. |
| `SENSOR A02YYUW` | Selects the SEN0311/A02YYUW UART sensor. |
| `SENSOR SEN0590` | Selects the SEN0590 I2C sensor. |
| `SENSOR AUTO` | Tries the supported sensor drivers automatically. |
| `READ` | Takes a fresh distance reading and reports calculated stage if calibrated. |
| `RAW` | Returns the fresh raw sensor distance without stage conversion. |
| `VERIFY` | Takes multiple fresh readings and reports median/range/spread for installation checks. |
| `INTERVAL 15MIN` | Saves a 15-minute automatic water-report interval. |
| `INTERVAL 1H` | Saves a 1-hour automatic water-report interval. |
| `CAL STAGE 1.42FT` | Calibrates using the independently measured stage and locks the calibration. |
| `CAL STATUS` | Shows calibration value, reference, lock state, and saved status. |
| `CAL LOCK` | Locks the current calibration against accidental replacement. |
| `CAL UNLOCK CONFIRM` | Unlocks calibration but keeps the existing calibration values. |
| `CAL RESET CONFIRM` | Clears only the water calibration; sensor choice and interval stay saved. |
| `TELEMETRY NOW` | Immediately sends a fresh water telemetry packet. |
| `RESET WATER CONFIRM` | Resets only the water subsystem to defaults; Meshtastic settings are preserved. |
| `POWER` | Reports battery/power status. |
| `WATCHDOG` | Reports the 90-second core watchdog, independent field-health channel, sleep/halt behavior, and reset reason. |
| `REBOOT` | Performs a safe non-destructive reboot after replying. |
| `RECOVER` | Alias for the safe recovery reboot. |

Commands are case-insensitive. A leading `/` is optional where supported.

## Safe flashing

Use the normal Seeed UF2 or BLE DFU package for routine updates. **Do not use factory-erase images for normal upgrades.**
