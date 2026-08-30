# CCA Heltec Sensor Gateway

**Canonical branch:** `cca-heltec-sensor-gateway`  
**Hardware:** Heltec WiFi LoRa 32 V4 OLED  
**PlatformIO target:** `heltec-v4`  
**Cloud path:** Heltec -> Vercel ingest -> Neon PostgreSQL -> dashboard

This branch is the single production gateway branch for the CCA Meshtastic sensor network. The Heltec remains a normal Meshtastic radio/server while also reading the Home HOBO, receiving remote telemetry, polling Fishlake, and uploading accepted readings over Wi-Fi.

## Station modes — important

The three HOBO stations do **not** use the same acquisition mode:

| Station | Mode | What causes a reading |
|---|---|---|
| **Hidden Valley** | **Automatic** | Remote RAK/HOBO node reads its logger and broadcasts standard environmental telemetry on its own schedule. |
| **Home** | **Automatic** | Heltec directly reads its local HOBO over BLE. Home readings are held locally for the cloud batch path. |
| **Fishlake Hightop** | **Heltec-triggered** | Heltec sends a Meshtastic DM `READ` to the Fishlake node; Fishlake replies with a fresh HOBO reading. |

**Fishlake is trigger/poll driven by the Heltec. Hidden Valley and Home are automatic.**

The Fishlake poller currently targets node `!5e021e35` and sends a `READ` every 60 minutes. The reply is parsed by the Heltec and uploaded as `Fishlake Hightop` telemetry. Fishlake therefore does not need to free-run its own HOBO telemetry broadcasts.

## Home + Hidden Valley cloud batching

Home local temperature is intentionally held instead of immediately waking the cloud backend. A Hidden Valley environmental temperature packet is the batch trigger:

```text
Home HOBO BLE read
      |
      v
held on Heltec
      |
      |  Hidden Valley automatic TELEMETRY_APP arrives
      v
one HTTPS array POST
[ Hidden Valley reading, held Home reading(s) ]
      |
      v
Vercel -> Neon
```

If no Home reading is pending, Hidden Valley still uploads normally. If the cloud request fails, held Home readings are restored for retry. Original Home observation timestamps are preserved.

Fishlake uses its own Heltec-triggered `READ`/reply/upload path and is not used as the Home batch trigger.

## Gateway responsibilities

The current branch preserves:

- normal Meshtastic LoRa radio/relay/client operation;
- Meshtastic TCP/API and web services;
- Wi-Fi connectivity and HTTPS upload;
- direct local HOBO BLE reading for Home;
- Hidden Valley standard environmental telemetry ingestion;
- Hidden Valley device/battery telemetry;
- Fishlake remote `READ` polling and reply parsing;
- existing sandstone/moisture/PIR packet compatibility for older field nodes;
- HOBO MX2001/MX2201/MX2203 support where implemented by the sensor adapters;
- Wi-Fi Unified OTA on Heltec V4.

## Architecture

```text
HIDDEN VALLEY (automatic) -------- TELEMETRY_APP ----+
                                                     |
HOME HOBO (automatic BLE) ----> HELTEC HOME <--------+
                                  |
                                  +---- DM READ ----> FISHLAKE (triggered)
                                  |<--- HOBO reply ---+
                                  |
                                  +---- Meshtastic radio/server
                                  |
                                  +---- Wi-Fi HTTPS ----> Vercel ----> Neon
```

## Fishlake trigger path

`FishlakePollerModule` is the owner of Fishlake acquisition on this branch.

Current behavior:

1. Heltec sends `READ` to Fishlake node `!5e021e35`.
2. Fishlake performs a fresh local HOBO read and returns a text reply.
3. Heltec accepts replies only from the configured Fishlake node.
4. Heltec parses the returned temperature/model information.
5. Heltec uploads the normalized reading as station `Fishlake Hightop`.
6. The scheduled trigger repeats every 60 minutes; failed sends are retried sooner.

This is intentionally different from Hidden Valley's automatic broadcast model.

## GitHub build

Use the dedicated workflow:

```text
Actions -> Build CCA Heltec Sensor Gateway -> Run workflow
```

It builds:

```text
heltec-v4 / esp32s3
```

The workflow injects `HOBO_HTTP_GATEWAY_INGEST_KEY` into the gateway build.

## Wi-Fi OTA

Normal Meshtastic TCP API:

```text
4403
```

ESP32 Unified OTA loader:

```text
3232
```

Use the regular `firmware-heltec-v4-*.bin` for routine OTA updates. Do not use the factory image for normal updates and do not erase NVS/configuration.

## Repository rules

1. `cca-heltec-sensor-gateway` is the single current Heltec gateway branch.
2. Hidden Valley and Home remain **automatic** acquisition paths.
3. Fishlake remains **Heltec-triggered/polled** unless the deployment design is intentionally changed.
4. Do not create location-specific Heltec firmware branches.
5. Keep existing packet/database formats backward-compatible unless the server side is migrated at the same time.
6. Keep ordinary Meshtastic radio/server behavior working while sensor features are added.
7. GitHub Actions is the normal build path; Wi-Fi OTA is the normal Heltec update path.

See [`docs/CCA_HELTEC_SENSOR_GATEWAY.md`](docs/CCA_HELTEC_SENSOR_GATEWAY.md) for the operational specification.
