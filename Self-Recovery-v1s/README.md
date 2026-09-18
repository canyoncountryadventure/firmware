# Self-Recovery v1 Firmware

Choose the exact board/application below. Every nRF52840 entry provides a USB UF2 and a BLE DFU ZIP.

| Firmware | Board | Documentation | USB | BLE / OTA |
|---|---|---|---|---|
| Seeed HOBO Safe v1 | Seeed XIAO nRF52840 + Wio-SX1262 | [README](Seeed-HOBO-Safe-v1/README.md) | [UF2](Seeed-HOBO-Safe-v1/Seeed-HOBO-Safe-v1.uf2) | [ZIP](Seeed-HOBO-Safe-v1/Seeed-HOBO-Safe-v1-OTA.zip) |
| RAK HOBO Safe v1 | RAK4631 + RAK19007 | [README](RAK-HOBO-Safe-v1/README.md) | [UF2](RAK-HOBO-Safe-v1/RAK-HOBO-Safe-v1.uf2) | [ZIP](RAK-HOBO-Safe-v1/RAK-HOBO-Safe-v1-OTA.zip) |
| Seeed Water Distance v1 | Seeed XIAO nRF52840 + Wio-SX1262 | [README](Seeed-Water-Distance-v1/README.md) | [UF2](Seeed-Water-Distance-v1/Seeed-Water-Distance-v1.uf2) | [ZIP](Seeed-Water-Distance-v1/Seeed-Water-Distance-v1-OTA.zip) |
| RAK Water Distance v1 | RAK4631 + RAK19007 | [README](RAK-Water-Distance-v1/README.md) | [UF2](RAK-Water-Distance-v1/RAK-Water-Distance-v1.uf2) | [ZIP](RAK-Water-Distance-v1/RAK-Water-Distance-v1-OTA.zip) |
| Seeed Water Distance + HOBO v1 | Seeed XIAO nRF52840 + Wio-SX1262 | [README](Seeed-Water-Distance-HOBO-v1/README.md) | [UF2](Seeed-Water-Distance-HOBO-v1/Seeed-Water-Distance-HOBO-v1.uf2) | [ZIP](Seeed-Water-Distance-HOBO-v1/Seeed-Water-Distance-HOBO-v1-OTA.zip) |
| RAK Water Distance + HOBO v1 | RAK4631 + RAK19007 | [README](RAK-Water-Distance-HOBO-v1/README.md) | [UF2](RAK-Water-Distance-HOBO-v1/RAK-Water-Distance-HOBO-v1.uf2) | [ZIP](RAK-Water-Distance-HOBO-v1/RAK-Water-Distance-HOBO-v1-OTA.zip) |
| RAK Soil Moisture + HOBO v1 | RAK4631 + RAK19007 + SEN0308 | [Full branch README](https://github.com/canyoncountryadventure/firmware/tree/RAK-Soil-Moisture-HOBO-v1) | [UF2](../downloads/RAK-Soil-Moisture-HOBO-v1.uf2) | [ZIP](../downloads/RAK-Soil-Moisture-HOBO-v1-OTA.zip) |
| Heltec Gateway v1 | Heltec V4 | [README](Heltec-Gateway-v1/README.md) | [Full build ZIP](Heltec-Gateway-v1/Heltec-Gateway-v1.zip) | — |

## Flashing rules

- **UF2:** double-press reset and copy the file to the board's USB bootloader drive.
- **OTA ZIP:** in Nordic nRF Connect choose **DFU → Distribution packet (ZIP)** and select the unopened ZIP.
- Do not use the UF2 in nRF Connect.
- Do not use factory-erase firmware for routine upgrades.
- Keep the LoRa antenna attached whenever the application may transmit.

Normal updates are intended to preserve node identity, channels, keys, NodeDB and saved sensor/logger configuration.
