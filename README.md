# Meshtastic Field Sensor Firmware

Custom Meshtastic firmware for unattended environmental monitoring, water-level stations, trail sensors, and the Heltec gateway.

The default branch, `field-self-recovery`, is the canonical recovery foundation and permanent-download branch. It also contains the shared HOBO self-recovery/watchdog source used by the HOBO-safe builds under `src/modules/Telemetry/HOBOSelfRecovery/`.

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

The download files are stored under `downloads/` on the default branch. Production workflows refresh their matching downloads after successful builds, so the links above remain stable.

## Canonical branches

These are the branches intended to remain in the repository after cleanup.

| Branch | Purpose | Board |
|---|---|---|
| `field-self-recovery` | Canonical recovery/watchdog foundation and permanent downloads | Seeed + RAK nRF52840 |
| `Seeed-HOBO-Safe-v1` | HOBO BLE only + self-recovery/watchdog | Seeed XIAO nRF52840 + Wio-SX1262 |
| `RAK-HOBO-Safe-v1` | HOBO BLE only + self-recovery/watchdog | RAK4631 + RAK19007 |
| `Heltec-Gateway-v1` | HOBO/mesh sensor gateway | Heltec V4 |
| `Seeed-Water-Distance-v1` | Water-level / stage only | Seeed XIAO nRF52840 + Wio-SX1262 |
| `RAK-Water-Distance-v1` | Water-level / stage only | RAK4631 + RAK19007 |
| `Seeed-Water-Distance-HOBO-v1` | Water-level / stage + HOBO BLE | Seeed XIAO nRF52840 + Wio-SX1262 |
| `RAK-Water-Distance-HOBO-v1` | Water-level / stage + HOBO BLE | RAK4631 + RAK19007 |
| `Trail-Sensors` | Consolidated PIR, rock telemetry, and SEN0171 trail-counter work | Seeed XIAO nRF52840 + Wio-SX1262 |

## HOBO Safe v1

The HOBO-only builds are for remote HOBO MX telemetry without water-distance logic. They retain the self-recovery supervisor, BLE recovery behavior, watchdog handling, remote recovery commands, battery/device telemetry, and preservation of normal Meshtastic configuration.

Shared recovery/watchdog source:

```text
src/modules/Telemetry/HOBOSelfRecovery/
```

HOBO telemetry source:

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

Core behavior includes fresh sensor reads, DM field setup, configurable report interval, field-stage calibration, calibration locking/reset, water telemetry, sensor-interface recovery, and redundant A/B persistent configuration with sequence, CRC32, exact-size validation, truncation-before-overwrite, and read-back verification.

### Standard field workflow

```text
STATUS
RAW
VERIFY
INTERVAL 1H
CAL STAGE 1.42FT
CAL STATUS
TELEMETRY NOW
```

Replace `1.42FT` with the independently measured stage at installation. After calibration, remove power completely, reconnect, then verify:

```text
STATUS
CAL STATUS
READ
```

To discard a bench/test calibration before field deployment:

```text
CAL RESET CONFIRM
```

## Combined Water + HOBO v1

The combined branches start from the corresponding Water Distance v1 firmware and add the universal HOBO BLE reader plus HOBO field-check/self-recovery helpers. They use the HOBO self-recovery module as the single recovery supervisor so watchdog/reboot handlers are not duplicated.

## Trail Sensors

`Trail-Sensors` consolidates the old trail-specific development branches into one place. It contains:

- SEN0171 PIR/presence station logic
- CCA rock telemetry
- HOBO support used by the PIR/rock remote-station build
- dedicated SEN0171 fast trail-counter firmware

The branch has separate PlatformIO targets for the PIR + Rock + HOBO station and the dedicated SEN0171 trail counter, so those implementations remain isolated at build time while sharing one maintained branch.

## Build policy

The project remains pinned to the validated **Meshtastic 2.7.26** base unless an upgrade is explicitly tested and approved. A successful compile is required but does not replace real hardware validation.

## Safe flashing

Use the board-specific UF2 or BLE DFU packages above for routine nRF52840 updates.

**Do not use factory-erase images for ordinary upgrades.** Normal updates are expected to preserve Meshtastic identity, channels, keys, NodeDB, and saved application settings.
