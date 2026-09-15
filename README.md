# Meshtastic Field Sensor Firmware

Custom Meshtastic firmware for unattended environmental monitoring and remote field nodes.

The repository is organized around a **self-recovery foundation** plus separate sensor/application branches. New field applications should inherit the recovery foundation instead of reimplementing watchdog, battery, solar, persistence, and remote diagnostics from scratch.

## Active branches

| Branch | Purpose | Board |
|---|---|---|
| `field-self-recovery` | Canonical recovery/base branch | Seeed + RAK nRF52840 platforms |
| `water-distance-seeed` | Water-level/stage node | Seeed XIAO nRF52840 + Wio-SX1262 |
| `water-distance-rak4631` | Water-level/stage node | RAK4631 + RAK19007 |

Trail-counter firmware is intentionally being separated from the water firmware. The water branches should not accumulate trail-specific logic.

Legacy and experimental branches may remain in Git history for reference, but they are not the preferred deployment targets unless explicitly documented as such.

## Recovery foundation

The field recovery baseline is designed to keep remote radios alive without destructive resets. It includes:

- nRF52840 watchdog supervision
- remote reboot/recovery commands
- battery/device telemetry
- low-voltage handling and solar recovery behavior
- persistent field settings
- BLE recovery behavior for logger applications
- preservation of Meshtastic identity, channels, keys, and NodeDB

Routine recovery and normal firmware updates must **not** factory-reset the radio.

## Water-distance firmware

The two active water branches are dedicated water-level/stage builds. They currently support:

- DFRobot SEN0313 / A01NYUB
- DFRobot SEN0311 / A02YYUW
- DFRobot SEN0590
- fresh UART-frame reads
- direct-message field setup
- stage calibration
- calibration lock/unlock/reset
- configurable report interval
- compact Meshtastic water telemetry
- sensor self-reinitialization after repeated read failures
- redundant persistent water configuration with CRC/read-back verification

Water mode is permanent in these branches; `MODE WATER` is not required.

## Water field workflow

Typical deployment sequence:

```text
STATUS
RAW
VERIFY
INTERVAL 1H
CAL STAGE 1.42FT
CAL STATUS
TELEMETRY NOW
```

Replace the example stage with the independently measured stage at the site.

After calibration, hard power-cycle the node and verify:

```text
STATUS
CAL STATUS
READ
```

The node is not considered field-ready until calibration and interval survive a complete power loss.

## Persistent water configuration

Water settings use redundant A/B records. Each record contains a sequence number and CRC32 and is verified after writing. The inactive record is overwritten first; the previous active record remains the fallback until the new record verifies.

The nRF52 Adafruit LittleFS implementation opens `FILE_O_WRITE` at end-of-file for existing files. The water branches therefore explicitly seek to offset zero and truncate the inactive record before writing. They also reject config files whose size is not exactly one record.

This prevents repeated saves from appending multiple configuration records into the same slot.

## A01NYUB power note

A01NYUB performs continuous ranging whenever it has power. Its blue sensor LED therefore continues blinking even when firmware only records/transmits once per hour.

To make the sensor itself sleep between readings, the hardware needs a load switch or high-side MOSFET so firmware can remove sensor power between scheduled measurements.

## Build policy

The project remains pinned to the validated Meshtastic 2.7.26 base unless an upgrade is explicitly tested and approved.

Board-specific GitHub Actions workflows build the water firmware for Seeed and RAK. A successful compile is necessary but does not replace field validation of sensor reads, persistence, calibration, telemetry, and recovery.

## Safe flashing

Use normal board-specific firmware, UF2, or BLE DFU packages for routine updates.

Do **not** use factory-erase images for ordinary upgrades. A normal update should preserve Meshtastic configuration and persistent field state.

## Source layout

Water sensor code lives under:

```text
src/modules/Telemetry/DistanceSensor/
```

Recovery code is shared from the field self-recovery foundation.

## Current development rule

Keep each deployment firmware narrow and explicit:

- water firmware does water-level/stage work
- trail firmware will do trail counting
- recovery infrastructure stays shared
- raw observations are preserved
- questionable data should be flagged rather than silently rewritten
