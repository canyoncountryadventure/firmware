# Field Self-Recovery v2 — Firmware Catalog

This is the canonical source baseline for the hardened Meshtastic field-firmware family.

Every active nRF52840 v2 target below has:

- its complete sensor-specific firmware;
- a normal, uncompressed UF2;
- an application-only Nordic BLE DFU ZIP;
- the direct-message `DFU` target hook;
- a verified persistent post-update callback marker;
- a separately built RAK4631 drone-flasher UF2 containing the compressed image for that exact configuration.

**Do not mix drone images between rows.** The drone UF2 embeds only the target firmware named in the same row.

## Firmware downloads

| Target configuration | Normal uncompressed UF2 | BLE DFU OTA ZIP | Matching compressed drone-flasher UF2 |
|---|---|---|---|
| Canonical Field Self-Recovery — RAK4631 | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Field-Self-Recovery-v2-RAK4631.uf2) | [OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Field-Self-Recovery-v2-RAK4631-OTA.zip) | [Drone UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Remote-Drone-Flashing-v2/downloads/Drone-Field-Self-Recovery-v2-RAK4631.uf2) |
| Canonical Field Self-Recovery — Seeed XIAO | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Field-Self-Recovery-v2-Seeed.uf2) | [OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Field-Self-Recovery-v2-Seeed-OTA.zip) | [Drone UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Remote-Drone-Flashing-v2/downloads/Drone-Field-Self-Recovery-v2-Seeed.uf2) |
| RAK HOBO Safe v2 | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-HOBO-Safe-v2.uf2) | [OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-HOBO-Safe-v2-OTA.zip) | [Drone UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Remote-Drone-Flashing-v2/downloads/Drone-RAK-HOBO-Safe-v2.uf2) |
| RAK Soil Moisture + HOBO v2 | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-Soil-Moisture-HOBO-v2.uf2) | [OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-Soil-Moisture-HOBO-v2-OTA.zip) | [Drone UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Remote-Drone-Flashing-v2/downloads/Drone-RAK-Soil-Moisture-HOBO-v2.uf2) |
| RAK Water Distance v2 | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-Water-Distance-v2.uf2) | [OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-Water-Distance-v2-OTA.zip) | [Drone UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Remote-Drone-Flashing-v2/downloads/Drone-RAK-Water-Distance-v2.uf2) |
| RAK Water Distance + HOBO v2 | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-Water-Distance-HOBO-v2.uf2) | [OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-Water-Distance-HOBO-v2-OTA.zip) | [Drone UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Remote-Drone-Flashing-v2/downloads/Drone-RAK-Water-Distance-HOBO-v2.uf2) |
| Seeed HOBO Safe v2 | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Seeed-HOBO-Safe-v2.uf2) | [OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Seeed-HOBO-Safe-v2-OTA.zip) | [Drone UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Remote-Drone-Flashing-v2/downloads/Drone-Seeed-HOBO-Safe-v2.uf2) |
| Seeed Water Distance v2 | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Seeed-Water-Distance-v2.uf2) | [OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Seeed-Water-Distance-v2-OTA.zip) | [Drone UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Remote-Drone-Flashing-v2/downloads/Drone-Seeed-Water-Distance-v2.uf2) |
| Trail PIR + Rock + HOBO v2 | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Trail-PIR-Rock-HOBO-v2.uf2) | [OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Trail-PIR-Rock-HOBO-v2-OTA.zip) | [Drone UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Remote-Drone-Flashing-v2/downloads/Drone-Trail-PIR-Rock-HOBO-v2.uf2) |
| Trail SEN0171 v2 | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Trail-SEN0171-v2.uf2) | [OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Trail-SEN0171-v2-OTA.zip) | [Drone UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Remote-Drone-Flashing-v2/downloads/Drone-Trail-SEN0171-v2.uf2) |

## Heltec gateway

Heltec V4 uses ESP32-S3 and cannot use the Nordic nRF52840 drone-DFU protocol.

| Configuration | Normal package | Drone DFU |
|---|---|---|
| Heltec Gateway v2 | [Download ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Heltec-Gateway-v2.zip) | Not compatible — ESP32-S3 |

## Drone update procedure

1. Initially install the target's normal UF2 by USB, or install its matching OTA ZIP through a supported Nordic BLE DFU client.
2. Direct-message the target node: `DFU`.
3. Wait for the target's confirmation that BLE OTA DFU is armed.
4. Flash the **drone UF2 from the same table row** onto the dedicated RAK4631 drone flasher.
5. Bring the drone flasher within BLE range. It scans for the target bootloader and performs the application-only update automatically.
6. After reboot, the target sends `UPDATE SUCCESS` or `DFU RESULT: BUILD UNCHANGED` over Meshtastic.

The normal target UF2 and the drone-flasher UF2 are different files. Never install a `Drone-*.uf2` directly onto the field sensor node.

## Recovery baseline

All v2 nRF52840 branches retain their own sensors and commands while sharing the hardened recovery layer:

- guarded SX1262 maintenance;
- missed RX/TX interrupt recovery;
- radio reinitialization and supported radio-rail power cycling;
- independent nRF52840 field-health watchdog;
- flash quiesce before intentional reset;
- reset-reason reporting;
- larger critical task stacks;
- scheduled preventive reboot fallback.

Engineering record: [Field Recovery v2 audit and build specification](https://github.com/canyoncountryadventure/firmware/blob/field-self-recovery/docs/FIELD_RECOVERY_V2_AUDIT_AND_BUILD_SPEC.md)
