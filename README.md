# Meshtastic Field Firmware

If you just want firmware, use **these two folders**:

1. **[`Self-Recovery-v1s/`](https://github.com/canyoncountryadventure/firmware/tree/field-self-recovery/Self-Recovery-v1s)** — HOBO, water-distance, combined water + HOBO, and Heltec gateway firmware.
2. **[`Trail-Sensors/`](https://github.com/canyoncountryadventure/firmware/tree/field-self-recovery/Trail-Sensors)** — trail-counter, PIR, rock telemetry, and trail HOBO firmware.

Everything else in the repository is source code or build infrastructure.

## Self-Recovery-v1s

| Firmware | Board | USB | BLE / OTA |
|---|---|---|---|
| **Seeed HOBO Safe v1** | Seeed XIAO nRF52840 + Wio-SX1262 | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Self-Recovery-v1s/Seeed-HOBO-Safe-v1/Seeed-HOBO-Safe-v1.uf2) | [ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Self-Recovery-v1s/Seeed-HOBO-Safe-v1/Seeed-HOBO-Safe-v1-OTA.zip) |
| **RAK HOBO Safe v1** | RAK4631 + RAK19007 | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Self-Recovery-v1s/RAK-HOBO-Safe-v1/RAK-HOBO-Safe-v1.uf2) | [ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Self-Recovery-v1s/RAK-HOBO-Safe-v1/RAK-HOBO-Safe-v1-OTA.zip) |
| **Seeed Water Distance v1** | Seeed XIAO nRF52840 + Wio-SX1262 | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Self-Recovery-v1s/Seeed-Water-Distance-v1/Seeed-Water-Distance-v1.uf2) | [ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Self-Recovery-v1s/Seeed-Water-Distance-v1/Seeed-Water-Distance-v1-OTA.zip) |
| **RAK Water Distance v1** | RAK4631 + RAK19007 | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Self-Recovery-v1s/RAK-Water-Distance-v1/RAK-Water-Distance-v1.uf2) | [ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Self-Recovery-v1s/RAK-Water-Distance-v1/RAK-Water-Distance-v1-OTA.zip) |
| **Seeed Water Distance + HOBO v1** | Seeed XIAO nRF52840 + Wio-SX1262 | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Self-Recovery-v1s/Seeed-Water-Distance-HOBO-v1/Seeed-Water-Distance-HOBO-v1.uf2) | [ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Self-Recovery-v1s/Seeed-Water-Distance-HOBO-v1/Seeed-Water-Distance-HOBO-v1-OTA.zip) |
| **RAK Water Distance + HOBO v1** | RAK4631 + RAK19007 | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Self-Recovery-v1s/RAK-Water-Distance-HOBO-v1/RAK-Water-Distance-HOBO-v1.uf2) | [ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Self-Recovery-v1s/RAK-Water-Distance-HOBO-v1/RAK-Water-Distance-HOBO-v1-OTA.zip) |
| **Heltec Gateway v1** | Heltec V4 | [Full build ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Self-Recovery-v1s/Heltec-Gateway-v1/Heltec-Gateway-v1.zip) | — |

### Seeed Water Distance + HOBO v1

This combined firmware is present and maintained in:

[`Self-Recovery-v1s/Seeed-Water-Distance-HOBO-v1/`](https://github.com/canyoncountryadventure/firmware/tree/field-self-recovery/Self-Recovery-v1s/Seeed-Water-Distance-HOBO-v1)

It combines the current Water Distance v1 logic with HOBO MX BLE telemetry and uses the HOBO self-recovery/watchdog supervisor. The water calibration persistence fix is retained.

## Trail-Sensors

| Firmware | Purpose | USB | BLE / OTA |
|---|---|---|---|
| **SEN0171 v1** | Dedicated fast trail counter | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Trail-Sensors/SEN0171/Trail-SEN0171-v1.uf2) | [ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Trail-Sensors/SEN0171/Trail-SEN0171-v1-OTA.zip) |
| **PIR + Rock + HOBO v1** | PIR / presence + rock telemetry + HOBO | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Trail-Sensors/PIR-Rock-HOBO/Trail-PIR-Rock-HOBO-v1.uf2) | [ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/Trail-Sensors/PIR-Rock-HOBO/Trail-PIR-Rock-HOBO-v1-OTA.zip) |

Both consolidated Trail-Sensors targets were compile-validated before the old standalone trail branches were removed.

## Water-distance field setup

The water builds support A01NYUB / SEN0313 by default, plus A02YYUW / SEN0311 and SEN0590. Typical field setup by Meshtastic DM:

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
- **BLE / OTA ZIP** = nRF Connect / BLE DFU update.
- **Do not use factory-erase images for routine upgrades.**

Normal updates are intended to preserve Meshtastic identity, channels, keys, NodeDB, and saved application settings.

The project remains pinned to the validated **Meshtastic 2.7.26** base unless an upgrade is explicitly tested and approved.
