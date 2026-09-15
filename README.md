# Meshtastic Field Sensor Firmware

Custom Meshtastic firmware for unattended environmental monitoring and remote field nodes.

The repository is organized around a **self-recovery foundation** plus narrow, board-specific production branches. New applications should inherit the recovery foundation instead of duplicating watchdog, battery, solar, persistence, and remote diagnostics.

## Active production branches

| Branch | Purpose | Board | Validation |
|---|---|---|---|
| `field-self-recovery` | Canonical recovery/base branch | Seeed + RAK nRF52840 | Shared foundation |
| `Seeed-Water-Distance-v1` | Water-level / stage only | Seeed XIAO nRF52840 + Wio-SX1262 | Hardware validated, including hard power-cycle persistence |
| `RAK-Water-Distance-v1` | Water-level / stage only | RAK4631 + RAK19007 | Build validated; RAK field hardware validation remains separate |
| `Seeed-Water-Distance-HOBO-v1` | Water-level / stage + HOBO BLE | Seeed XIAO nRF52840 + Wio-SX1262 | Combined build; validate assembled field hardware before deployment |
| `RAK-Water-Distance-HOBO-v1` | Water-level / stage + HOBO BLE | RAK4631 + RAK19007 | Combined build; validate assembled field hardware before deployment |

Trail-counter firmware is intentionally separate from the water firmware. Water branches should not accumulate trail-specific logic.

## Water Distance v1

The production water firmware is permanently water-only; `MODE WATER` is not required.

Supported distance sensors:

- DFRobot SEN0313 / A01NYUB — default
- DFRobot SEN0311 / A02YYUW
- DFRobot SEN0590

Core behavior:

- fresh UART-frame reads
- direct-message field setup
- configurable report interval
- stage calibration from an independently measured field stage
- automatic calibration lock
- calibration unlock/reset commands
- compact Meshtastic water telemetry on channel 0
- sensor-interface reinitialization after repeated read failures
- redundant persistent A/B configuration with sequence, CRC32, exact-size validation, truncation-before-overwrite, and read-back verification

## Combined Water + HOBO branches

The combined branches start from the corresponding Water Distance v1 branch and add the existing universal HOBO BLE reader for MX2001/MX2201/MX2203 plus HOBO field-check/self-recovery helpers.

The combined builds intentionally use the **HOBO self-recovery module as the single recovery supervisor** rather than also instantiating the standalone distance recovery supervisor. This avoids duplicate watchdog/reboot command handlers while retaining watchdog, BLE recovery, remote recovery commands, and water-config persistence.

## Standard water field workflow

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

Replace `1.42FT` with the independently measured water stage at the installation site.

After calibration, remove power completely, reconnect it, then verify:

```text
STATUS
CAL STATUS
READ
```

The node is not deployment-ready until calibration and interval survive the hard power cycle.

To discard a bench/test calibration before field deployment:

```text
CAL RESET CONFIRM
```

This clears only water calibration while preserving sensor selection and report interval.

## Recovery foundation

The shared recovery baseline includes:

- nRF52840 watchdog supervision
- remote reboot/recovery commands
- battery/device telemetry
- low-voltage handling and solar recovery behavior
- persistent field settings
- BLE recovery behavior where applicable
- preservation of Meshtastic identity, channels, keys, and NodeDB

Routine recovery and ordinary firmware updates must **not** factory-reset a field node.

## Persistent water configuration

Water settings use redundant A/B flash records. The inactive record is rewound and truncated before writing, then re-opened and checked for exact size, sequence, CRC32, and valid contents before it becomes active. The previous valid slot remains the fallback until the new record passes verification.

Saved water configuration is intended to survive battery removal, solar shutdown/recovery, reboot, watchdog reset, and normal non-destructive firmware updates.

## A01NYUB power note

A01NYUB performs continuous ranging whenever powered, so its blue LED keeps blinking even when firmware records/transmits only once per hour.

Actual sensor sleep between scheduled readings requires hardware power gating with a load switch or high-side MOSFET.

## Build policy

The project remains pinned to the validated **Meshtastic 2.7.26** base unless an upgrade is explicitly tested and approved.

Each production branch has a board-specific GitHub Actions build. A successful compile is required but does not replace real hardware validation of sensor reads, persistence, calibration, telemetry, BLE behavior, and recovery.

## Safe flashing

Use normal board-specific UF2 or BLE DFU packages for routine updates.

**Do not use factory-erase images for ordinary upgrades.** Normal updates are expected to preserve Meshtastic configuration and saved water settings.

## Source layout

Water sensor code:

```text
src/modules/Telemetry/DistanceSensor/
```

HOBO telemetry/recovery code:

```text
src/modules/Telemetry/HOBOMX2001MX2201MX2203/
src/modules/Telemetry/HOBOMX2201MX2001/
src/modules/Telemetry/HOBOSelfRecovery/
```

## Development rules

- Water firmware does water-level/stage work only.
- Combined water + HOBO firmware adds only the HOBO BLE path and its recovery helpers.
- Trail firmware is maintained separately.
- Recovery infrastructure stays shared.
- Raw observations are preserved.
- Questionable data should be flagged rather than silently rewritten.
