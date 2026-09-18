# Meshtastic Field Firmware

If you just want firmware, start here:

1. **[`Self-Recovery-v1s/`](https://github.com/canyoncountryadventure/firmware/tree/field-self-recovery/Self-Recovery-v1s)** — HOBO, water-distance, combined water + HOBO, and Heltec gateway firmware.
2. **[`Trail-Sensors/`](https://github.com/canyoncountryadventure/firmware/tree/field-self-recovery/Trail-Sensors)** — trail-counter, PIR, rock telemetry, and trail HOBO firmware.
3. **[`RAK-Soil-Moisture-HOBO-v1`](https://github.com/canyoncountryadventure/firmware/tree/RAK-Soil-Moisture-HOBO-v1)** — RAK4631/RAK19007 + SEN0308 soil moisture + optional HOBO firmware.
4. **[`Remote-Drone-Flashing`](https://github.com/canyoncountryadventure/firmware/tree/Remote-Drone-Flashing)** — autonomous RAK4631-to-RAK4631 BLE firmware-update prototype and matched build workflow.

Everything else in the repository is source code or build infrastructure.

## Start with the right file

| File type | Use it for | How |
|---|---|---|
| `.uf2` | Normal USB installation and recovery | Double-press reset, then copy the UF2 to the board's bootloader drive. |
| `-OTA.zip` | Android BLE update | In Nordic nRF Connect choose **DFU → Distribution packet (ZIP)**. Do not unzip it. |
| Full build `.zip` | Platform-specific bundle such as the Heltec gateway | Open its firmware folder and follow that build's README. |

Do not select a UF2 in nRF Connect, and do not copy an OTA ZIP to the bootloader drive. Routine upgrades should not use factory-erase images.

## Self-Recovery-v1s

| Firmware | Board | USB | BLE / OTA |
|---|---|---|---|
| **Seeed HOBO Safe v1** | Seeed XIAO nRF52840 + Wio-SX1262 | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Self-Recovery-v1s/Seeed-HOBO-Safe-v1/Seeed-HOBO-Safe-v1.uf2) | [ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Self-Recovery-v1s/Seeed-HOBO-Safe-v1/Seeed-HOBO-Safe-v1-OTA.zip) |
| **RAK HOBO Safe v1** | RAK4631 + RAK19007 | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Self-Recovery-v1s/RAK-HOBO-Safe-v1/RAK-HOBO-Safe-v1.uf2) | [ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Self-Recovery-v1s/RAK-HOBO-Safe-v1/RAK-HOBO-Safe-v1-OTA.zip) |
| **Seeed Water Distance v1** | Seeed XIAO nRF52840 + Wio-SX1262 | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Self-Recovery-v1s/Seeed-Water-Distance-v1/Seeed-Water-Distance-v1.uf2) | [ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Self-Recovery-v1s/Seeed-Water-Distance-v1/Seeed-Water-Distance-v1-OTA.zip) |
| **RAK Water Distance v1** | RAK4631 + RAK19007 | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Self-Recovery-v1s/RAK-Water-Distance-v1/RAK-Water-Distance-v1.uf2) | [ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Self-Recovery-v1s/RAK-Water-Distance-v1/RAK-Water-Distance-v1-OTA.zip) |
| **Seeed Water Distance + HOBO v1** | Seeed XIAO nRF52840 + Wio-SX1262 | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Self-Recovery-v1s/Seeed-Water-Distance-HOBO-v1/Seeed-Water-Distance-HOBO-v1.uf2) | [ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Self-Recovery-v1s/Seeed-Water-Distance-HOBO-v1/Seeed-Water-Distance-HOBO-v1-OTA.zip) |
| **RAK Water Distance + HOBO v1** | RAK4631 + RAK19007 | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Self-Recovery-v1s/RAK-Water-Distance-HOBO-v1/RAK-Water-Distance-HOBO-v1.uf2) | [ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Self-Recovery-v1s/RAK-Water-Distance-HOBO-v1/RAK-Water-Distance-HOBO-v1-OTA.zip) |
| **RAK Soil Moisture + HOBO v1** | RAK4631 + RAK19007 + SEN0308 | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-Soil-Moisture-HOBO-v1.uf2) | [ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-Soil-Moisture-HOBO-v1-OTA.zip) |
| **Heltec Gateway v1** | Heltec V4 | [Full build ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Self-Recovery-v1s/Heltec-Gateway-v1/Heltec-Gateway-v1.zip) | — |

Detailed soil wiring, calibration, packet format, commands, validation and troubleshooting are on the **[RAK Soil Moisture + HOBO v1 branch front page](https://github.com/canyoncountryadventure/firmware/tree/RAK-Soil-Moisture-HOBO-v1)**.

## Trail-Sensors

| Firmware | Purpose | USB | BLE / OTA |
|---|---|---|---|
| **SEN0171 v1** | Dedicated fast trail counter | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Trail-Sensors/SEN0171/Trail-SEN0171-v1.uf2) | [ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Trail-Sensors/SEN0171/Trail-SEN0171-v1-OTA.zip) |
| **PIR + Rock + HOBO v1** | PIR / presence + rock telemetry + HOBO | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Trail-Sensors/PIR-Rock-HOBO/Trail-PIR-Rock-HOBO-v1.uf2) | [ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Trail-Sensors/PIR-Rock-HOBO/Trail-PIR-Rock-HOBO-v1-OTA.zip) |

## Useful DM commands

Send these as direct Meshtastic text messages to the target node. Commands are case-insensitive; a leading `/` is optional where supported.

### HOBO Safe v1 — Seeed or RAK

| Command | What it does |
|---|---|
| `STATUS` | Shows overall node/HOBO health. |
| `LOGGER` | Shows HOBO model, MAC, BLE RSSI, interval, and lock state. |
| `READ` | Takes a fresh HOBO reading without consuming the automatic pointer. |
| `LOCK` | Saves the identified HOBO as this station's logger. |
| `UNLOCK` | Clears the saved logger and resumes discovery. |
| `BLE` | Shows BLE scanning/link/recovery state. |
| `AUTO` | Shows automatic pointer-gated HOBO record state. |
| `POWER` | Shows battery voltage, percentage, and charging state. |
| `WATCHDOG` | Shows watchdog state/ownership. |
| `PING` | Quick DM/liveness test. |
| `SCAN` | Refreshes disconnected BLE scanning. |
| `RECONNECT` | Rebuilds the disconnected BLE scanner/link. |
| `RECOVER` or `REBOOT` | Replies, then safely reboots without erasing Meshtastic settings. |

### Water Distance v1 — Seeed or RAK

| Command | What it does |
|---|---|
| `STATUS` | Shows sensor, interval, calibration, and readiness. |
| `RAW` | Takes a fresh raw distance reading. |
| `READ` | Takes a fresh reading and reports calculated stage if calibrated. |
| `VERIFY` | Takes multiple fresh readings and reports median/range/spread. |
| `SENSOR A01NYUB` | Selects the SEN0313/A01NYUB sensor. |
| `SENSOR A02YYUW` | Selects the SEN0311/A02YYUW sensor. |
| `SENSOR SEN0590` | Selects the SEN0590 sensor. |
| `SENSOR AUTO` | Automatically tries supported distance-sensor drivers. |
| `INTERVAL 1H` | Saves a 1-hour automatic report interval. |
| `CAL STAGE 1.42FT` | Calibrates from the independently measured stage and locks calibration. |
| `CAL STATUS` | Shows calibration/reference/lock state. |
| `CAL UNLOCK CONFIRM` | Unlocks calibration without erasing it. |
| `CAL RESET CONFIRM` | Clears calibration only; sensor and interval remain saved. |
| `TELEMETRY NOW` | Immediately sends a fresh water telemetry packet. |
| `RESET WATER CONFIRM` | Resets only the water subsystem to defaults. |
| `POWER` | Shows battery/power status. |
| `WATCHDOG` | Shows watchdog status. |
| `RECOVER` or `REBOOT` | Safely reboots without factory-erasing the node. |

### Combined Water Distance + HOBO v1

Use the explicit `WATER` prefix for water commands to avoid ambiguity:

| Command | What it does |
|---|---|
| `WATER STATUS` | Shows water sensor/calibration/readiness. |
| `WATER READ` | Takes a fresh water reading. |
| `WATER RAW` | Shows raw water-sensor distance. |
| `WATER VERIFY` | Runs the multi-reading installation check. |
| `WATER INTERVAL 1H` | Saves hourly water telemetry. |
| `WATER CAL STAGE 1.42FT` | Calibrates and locks water stage. |
| `WATER CAL STATUS` | Shows saved water calibration state. |
| `WATER CAL RESET CONFIRM` | Clears only water calibration. |
| `WATER TELEMETRY NOW` | Sends water telemetry immediately. |
| `LOGGER` | Shows the HOBO logger identity/state. |
| `READ` | Requests a fresh HOBO reading. |
| `LOCK` / `UNLOCK` | Saves or clears the HOBO logger assignment. |
| `BLE` | Shows HOBO BLE state. |
| `SCAN` / `RECONNECT` | Refreshes or rebuilds the disconnected HOBO BLE path. |
| `POWER` | Shows battery/power state. |
| `WATCHDOG` | Shows watchdog state. |
| `RECOVER` / `REBOOT` | Safely reboots the node. |

### RAK Soil Moisture + HOBO v1

SEN0308 wiring on the RAK19007:

| SEN0308 lead | RAK19007 |
|---|---|
| Red | `VDD` (regulated 3.3 V; **not VBAT**) |
| Yellow | `AIN1` |
| Black | `GND` |
| Black | `GND` |

`AIN1` maps to RAK4631 A1 / P0.31 / AIN7. Current soil calibration is ADC10 580 = 0% and ADC10 0 = 100%, with 20 samples averaged per reading and automatic telemetry once per hour.

| Command | What it does |
|---|---|
| `SOIL` / `SOIL READ` | Fresh DM-only moisture percentage and raw ADC10. |
| `SOIL STATUS` | Last reading, one-hour interval, input pin and endpoints. |
| `SOIL TX` | Fresh reading plus standard soil telemetry and compact raw packet. |
| `SOIL CAL` | Shows the fixed endpoints and direction. |
| `SOIL HELP` | Lists soil commands. |
| `LOGGER` / `READ` | Shows or reads the optional HOBO logger. |
| `LOCK` / `UNLOCK` | Saves or clears the HOBO assignment. |
| `WATCHDOG` | Shows the live nRF52840 watchdog state. |

The percentage is a field-calibrated index, not laboratory volumetric water content. The standard `soil_moisture` telemetry is app-compatible; the raw `PRIVATE_APP` packet retains ADC10 for custom ingestion and later recalibration.

### Trail PIR + Rock + HOBO

| Command | What it does |
|---|---|
| `STATUS` | Shows uptime, battery, PIR state/counts, alert destination, and HOBO hint. |
| `ALERTS HERE` | Saves the sender as the private PIR/power/boot alert destination. |
| `ALERTS STATUS` | Shows the saved alert destination. |
| `ALERTS CLEAR` | Clears the private alert destination. |
| `PIR STATUS` | Shows live PIR state, totals, last detection, and TX state. |
| `PIR COUNT` | Shows cumulative and since-boot detections. |
| `PIR ON` / `PIR OFF` | Enables or disables PIR monitoring persistently. |
| `PIR TX ON` / `PIR TX OFF` | Enables or disables private PIR alert transmissions. |
| `POWER` | Shows battery state and trend. |
| `POWER HISTORY` | Shows stored voltage checkpoints/min/max. |
| `LOGGER` | Shows the HOBO logger identity/state. |
| `READ` | Takes a fresh HOBO reading. |
| `LOCK` / `UNLOCK` | Saves or clears the HOBO assignment. |

The dedicated **SEN0171 trail-counter** build currently has no custom plain-text DM command parser; it automatically sends `PERSON WALKED BY #...` messages for detected events.

### Heltec Gateway v1

The current Heltec gateway does not yet expose custom gateway-control text DMs. Planned direct-HOBO commands are `LOGGER`, `READ`, `LOCK`, and `UNLOCK`; do not rely on them until direct HOBO BLE support is merged and validated.

## Water-distance field setup

The water builds support A01NYUB / SEN0313 by default, plus A02YYUW / SEN0311 and SEN0590. A typical field sequence is:

```text
STATUS
RAW
VERIFY
INTERVAL 1H
CAL STAGE 1.42FT
CAL STATUS
TELEMETRY NOW
```

Replace `1.42FT` with the independently measured stage. After calibration, fully remove power, reconnect, then verify `STATUS`, `CAL STATUS`, and `READ`.

To discard a bench calibration without changing the saved interval:

```text
CAL RESET CONFIRM
```

## Safe flashing

For nRF52840 boards:

- **UF2** = normal USB drag-and-drop firmware update.
- **BLE / OTA ZIP** = Nordic nRF Connect **Distribution packet (ZIP)**; leave the ZIP unopened.
- **Do not use factory-erase images for routine upgrades.**

Normal updates are intended to preserve Meshtastic identity, channels, keys, NodeDB, and saved application settings.

If a legacy BLE update reaches 100% but stalls during validation or activation, recover with the matching UF2 over USB rather than repeatedly forcing BLE attempts.

## Remote Drone Flashing

The **[Remote-Drone-Flashing branch](https://github.com/canyoncountryadventure/firmware/tree/Remote-Drone-Flashing)** builds a matched pair:

- a normal RAK4631 Meshtastic + HOBO target with the `DFU` DM command; and
- a dedicated BLE-only RAK4631 Scout carrying a compressed copy of that exact target application.

Use the **[latest successful Remote Drone Flasher workflow run](https://github.com/canyoncountryadventure/firmware/actions/workflows/build_remote_drone_flasher.yml?query=branch%3ARemote-Drone-Flashing)** and download its `remote-drone-flashing-<run>` artifact. Keep the target and Scout files from the same run together. The package contains:

```text
RAK4631-HOBO-DFU-Target.uf2
RAK4631-HOBO-DFU-Target-OTA.zip
RAK4631-Remote-Drone-Flasher.uf2
BUILD.txt
```

Bench validation has proven the autonomous embedded-image transfer, validation, activation, target reboot and LoRa callback using a same-build reflash. A true different-build callback and an airborne hover/update mission remain unproven. Read the branch README before treating it as field-ready.

## Common RAK three-blink diagnosis

Three LED flashes, a pause, then three flashes again means the nRF52840 measured its regulated VDD rail below the firmware's **2.7-V safe-boot threshold**. The application stops before Meshtastic, sensor code and the USB console start.

1. Disconnect battery, solar and sensors.
2. Leave the LoRa antenna attached.
3. Power the RAK19007 from a known-good 5-V USB-C source and press reset once.
4. If the pattern continues, remove power, reseat the RAK4631 firmly, and retry.

The project remains pinned to the validated **Meshtastic 2.7.26** base unless an upgrade is explicitly tested and approved.
