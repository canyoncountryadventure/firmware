# Meshtastic Field Sensor Firmware

Custom Meshtastic firmware for unattended environmental monitoring and remote field nodes.

The default branch, `field-self-recovery`, is the canonical recovery foundation. It contains the shared HOBO self-recovery/watchdog source used by the HOBO-safe builds, including `src/modules/Telemetry/HOBOSelfRecovery/`.

## Download firmware

These are the production firmware packages intended to remain on the front page.

For nRF52840 boards, **UF2** is for normal USB drag-and-drop flashing and **BLE / OTA ZIP** is for nRF Connect / BLE DFU. These are normal update images; do **not** use factory-erase firmware for routine updates.

| Firmware | USB UF2 | BLE / OTA ZIP |
|---|---|---|
| **Seeed HOBO Safe v1** | [Download UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Seeed-HOBO-Safe-v1.uf2) | [Download ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Seeed-HOBO-Safe-v1-OTA.zip) |
| **RAK HOBO Safe v1** | [Download UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-HOBO-Safe-v1.uf2) | [Download ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-HOBO-Safe-v1-OTA.zip) |
| **Seeed Water Distance v1** | [Download UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Seeed-Water-Distance-v1.uf2) | [Download ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Seeed-Water-Distance-v1-OTA.zip) |
| **RAK Water Distance v1** | [Download UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-Water-Distance-v1.uf2) | [Download ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-Water-Distance-v1-OTA.zip) |
| **Seeed Water Distance + HOBO v1** | [Download UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Seeed-Water-Distance-HOBO-v1.uf2) | [Download ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Seeed-Water-Distance-HOBO-v1-OTA.zip) |
| **RAK Water Distance + HOBO v1** | [Download UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-Water-Distance-HOBO-v1.uf2) | [Download ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-Water-Distance-HOBO-v1-OTA.zip) |

### Heltec V4 gateway

**[Download Heltec Gateway v1 full build ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Heltec-Gateway-v1.zip)**

The download files are stored under `downloads/` on the default branch. Each production workflow refreshes its matching download after a successful build, so these front-page links stay stable.

## Active production branches

| Branch | Purpose | Board | Validation |
|---|---|---|---|
| `field-self-recovery` | Canonical recovery/watchdog foundation and permanent downloads | Seeed + RAK nRF52840 | Shared foundation |
| `Seeed-HOBO-Safe-v1` | HOBO BLE only + self-recovery/watchdog | Seeed XIAO nRF52840 + Wio-SX1262 | Based on latest successful HOBO self-recovery Seeed build |
| `RAK-HOBO-Safe-v1` | HOBO BLE only + self-recovery/watchdog | RAK4631 + RAK19007 | Based on latest successful HOBO self-recovery RAK build |
| `Heltec-Gateway-v1` | HOBO/mesh sensor gateway | Heltec V4 | Canonicalized from the latest clean Heltec gateway build |
| `Seeed-Water-Distance-v1` | Water-level / stage only | Seeed XIAO nRF52840 + Wio-SX1262 | Hardware validated, including hard power-cycle persistence |
| `RAK-Water-Distance-v1` | Water-level / stage only | RAK4631 + RAK19007 | Build validated; RAK field hardware validation remains separate |
| `Seeed-Water-Distance-HOBO-v1` | Water-level / stage + HOBO BLE | Seeed XIAO nRF52840 + Wio-SX1262 | Combined build; validate assembled field hardware before deployment |
| `RAK-Water-Distance-HOBO-v1` | Water-level / stage + HOBO BLE | RAK4631 + RAK19007 | Combined build; validate assembled field hardware before deployment |

## HOBO Safe v1

The HOBO-only builds are for remote HOBO MX telemetry without water-distance logic. They retain the self-recovery supervisor, BLE recovery behavior, watchdog handling, remote recovery commands, battery/device telemetry, and preservation of normal Meshtastic configuration.

The shared recovery/watchdog source is maintained on the default branch under:

```text
src/modules/Telemetry/HOBOSelfRecovery/
```

HOBO telemetry support lives under:

```text
src/modules/Telemetry/HOBOMX2001MX2201MX2203/
src/modules/Telemetry/HOBOMX2201MX2001/
```

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

## Combined Water + HOBO v1

The combined branches start from the corresponding Water Distance v1 firmware and add the universal HOBO BLE reader for MX2001/MX2201/MX2203 plus HOBO field-check/self-recovery helpers.

The combined builds use the **HOBO self-recovery module as the single recovery supervisor** rather than also instantiating the standalone distance recovery supervisor. This avoids duplicate watchdog/reboot handlers while retaining watchdog, BLE recovery, remote recovery commands, and water-config persistence.

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

## Persistent water configuration

Water settings use redundant A/B flash records. The inactive record is rewound and truncated before writing, then re-opened and checked for exact size, sequence, CRC32, and valid contents before it becomes active. The previous valid slot remains the fallback until the new record passes verification.

Saved water configuration is intended to survive battery removal, solar shutdown/recovery, reboot, watchdog reset, and normal non-destructive firmware updates.

## A01NYUB power note

A01NYUB performs continuous ranging whenever powered, so its blue LED keeps blinking even when firmware records/transmits only once per hour.

Actual sensor sleep between scheduled readings requires hardware power gating with a load switch or high-side MOSFET.

## Build policy

The project remains pinned to the validated **Meshtastic 2.7.26** base unless an upgrade is explicitly tested and approved.

A successful compile is required but does not replace real hardware validation of sensor reads, persistence, calibration, telemetry, BLE behavior, gateway behavior, and recovery.

## Safe flashing

Use the board-specific UF2 or BLE DFU packages above for routine nRF52840 updates.

**Do not use factory-erase images for ordinary upgrades.** Normal updates are expected to preserve Meshtastic identity, channels, keys, NodeDB, and saved application settings.
