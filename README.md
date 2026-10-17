# CCA Heltec Sensor Gateway V5
**October 17, 2026 · Heltec WiFi LoRa 32 V4 OLED · ESP32-S3**

This is the dedicated new V5 gateway branch, replacing V4 as the source for new CCA gateway firmware. The [old Heltec V4](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v4) remains untouched for rollback. Hardware remains **V4 OLED**, despite the *firmware* now being V5. Use PlatformIO environment `heltec-v4`, never the TFT environment for the OLED board.

## Firmware downloads

The following links refer to files **actually generated and committed** by the V5 workflow. Filename dates are part of the downloaded files, not merely labels.

- [Heltec Gateway V5, October 17, 2026, ESP32-S3 firmware BIN](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Heltec-Gateway-v5/downloads/Heltec-Gateway-v5/Heltec-Gateway-v5-10-17-26.bin)
- [Heltec Gateway V5, October 17, 2026, complete flash/OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Heltec-Gateway-v5/downloads/Heltec-Gateway-v5/Heltec-Gateway-v5-10-17-26.zip)
- [Build source and SHA-256 checksums](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Heltec-Gateway-v5/downloads/Heltec-Gateway-v5/BUILD.txt)
- [Successful GitHub Actions build](https://github.com/canyoncountryadventure/firmware/actions/workflows/build_cca_heltec_gateway.yml?query=branch%3AHeltec-Gateway-v5)

**Heltec ESP32-S3 uses BIN, not a RAK UF2.** The ZIP contains the companion flashing/OTA image set. Do not flash a RAK UF2 onto Heltec hardware or treat a RAK drone Scout image as a Heltec update. A successful CI build proves software compilation; validate BLE/logger/cloud telemetry on hardware.

## V5 upgrade summary

- **Reads RAK HOBO V6's new precise packet.** Accepts V1 `MX`/19-byte stage and temperature *tenths* from previously deployed radios and V2 `MX`/21-byte signed stage32 and temperature16 *hundredths* from RAK V6, automatically selected by explicit payload version and length. No loss of the 0.01 digit during gateway decoding.
- **Direct Heltec BLE HOBO MX2001 data** is likewise transmitted at hundredths on the mesh; direct local Vercel ingestion retains three-digit numeric JSON precision. MX2201/MX2203 temperature environmental telemetry remains supported.
- **Distance sensors** keep their 24-byte `DS` packet pathway, including calibrated water stage/distance. Existing moisture/PIR (`RK`), soil (`SM`), environmental and device battery telemetry remain compatible.
- **Meshtastic forwarding and HTTPS cloud ingest** retain current CCA node IDs and data format. Remote RAK V6's HUB SET changes where the field node directs its telemetry; it does not configure the Heltec or choose a forced relay.
- **Previous Heltec V4 recovery preserved:** watchdog health, bounded BLE connect/GATT worker, 12-hour maintenance reboot, SX1262 recovery and local/retained diagnostic breadcrumbs. No new factory erase or credential change required for application-only updates.

## Gateway capabilities

| Inbound measurement | Packet path | Output |
| --- | --- | --- |
| HOBO MX2001 stage + temperature (legacy RAK) | Private `MX` version 1, 19 bytes, tenths | Vercel/Neon plus mesh |
| HOBO MX2001 stage + temperature (RAK V6) | Private `MX` version 2, 21 bytes, hundredths | Vercel/Neon plus mesh |
| Direct local HOBO MX2001 via Heltec BLE | BLE logger, V2 `MX` mesh packet + local queue | Vercel/Neon plus mesh |
| HOBO MX2201/MX2203 | Environmental telemetry | Vercel/Neon plus mesh |
| RAK SEN0313/MB7388 distance/stage | `DS` private packet | Vercel/Neon |
| Moisture/PIR/soil | `RK` / `SM` private packet | Vercel/Neon |
| Meshtastic battery / voltage / channel use / uptime | Device telemetry | Vercel/Neon |

### Heltec direct-message commands

`LOGGER` checks direct BLE logger model, MAC, RSSI, connection and saved target. `READ` requests a new logger measurement. `LOCK` or `LOCK AA:BB:CC:DD:EE:FF` keeps the logger assignment after reboot; `UNLOCK` clears the lock and resumes discovery. `DIAG` or `CRASHLOG` returns previous-reset details and failure breadcrumbs **by DM**, not just a serial log. `CLEAR DIAG` clears the saved Heltec diagnostic snapshot. `WATCHDOG` reports the 90-second task watchdog and 12-hour preventive reboot schedule.

These direct HOBO commands are separate from the RAK station's `SAMPLES`, `WATER ...`, `HUB SET`, `LAST RESORT` and recovery commands. Send station controls to the RAK node, not to the Heltec gateway.

## Cloud architecture

```text
RAK SEN0313 / MB7388 + HOBO MX2001    Other Meshtastic field nodes
               |                                  |
          Meshtastic LoRa  <-----------------------+
               |
         HELTEC V4 OLED RUNNING V5
             |       |
       Local BLE     HTTPS ingest (/api/ingest)
       HOBO option           |
                         Vercel API
                            |
                           Neon
                            |
                     Watershed dashboard
```

Gateway Wi-Fi is used for HTTPS/Neon ingestion and OTA, not a replacement for Meshtastic LoRa relaying. The GitHub Actions reusable build injects the `HOBO_HTTP_GATEWAY_INGEST_KEY` repository secret. Verify that a real reading reaches Neon/dashboard before field deployment.

## Updating and rollback

- Routine ESP32-S3 updates should retain NVS, channel keys, Wi-Fi provisioning, BLE bonds and the existing Meshtastic Unified OTA loader. **Do not erase all flash** unless explicitly troubleshooting configuration.
- Meshtastic TCP API normally uses **4403**; ESP32 Unified OTA loader normally uses **3232**.
- The dedicated [V5 GitHub Actions workflow](.github/workflows/build_cca_heltec_gateway.yml) compiles the real OLED target `heltec-v4`, builds dated BIN/ZIP files, records the source SHA and checksums, and publishes only after CI success. A later README-only build changes the provenance, not the hardware target.
- Keep a [Heltec V4 rollback image](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v4) and [RAK V5 station release](https://github.com/canyoncountryadventure/firmware/tree/RAK-Water-Distance-HOBO-v5) accessible separately. Never mix field and gateway flash images.

**Field acceptance checklist:** mesh receive/transmit; Heltec Wi-Fi/cloud ingest; remote V1 and V2 MX2001; real hundredths at dashboard (no truncation by Vercel/Neon); distance stage and device voltage/percentage; direct Heltec logger BLE scan/model/lock/read; watchdog and `DIAG` DM; successful OTA reboot. GitHub Actions does not exercise physical radio or cloud end-to-end.

See [full gateway operational specification](docs/CCA_HELTEC_SENSOR_GATEWAY.md) and [RAK V6 matching field firmware](https://github.com/canyoncountryadventure/firmware/tree/RAK-Water-Distance-HOBO-v6).
