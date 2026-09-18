> **Heltec Gateway v2:** gateway ingestion, mesh forwarding, Wi-Fi OTA, and Vercel/Neon behavior are retained. The shared SX1262 v2 recovery changes are included; direct local HOBO BLE remains a future gateway feature.

# CCA Heltec Sensor Gateway v2

**Canonical Heltec branch:** `Heltec-Gateway-v2`  
**Gateway hardware:** Heltec WiFi LoRa 32 V4 OLED  
**PlatformIO target:** `heltec-v4`  
**Cloud path:** Heltec -> Vercel ingest -> Neon PostgreSQL -> dashboard

This branch is the single source of truth for the CCA Heltec gateway. It is intentionally sensor-agnostic: existing sensor inputs stay supported as new sensor types are added.

## What is preserved

The gateway currently preserves the working paths for:

- sandstone/moisture + PIR packets from field nodes;
- HOBO MX2001 water-level + temperature packets;
- standard Meshtastic environmental temperature telemetry, including MX2201 temperature;
- Meshtastic device telemetry, including battery percentage/voltage used by the Hidden Valley monitoring path;
- mesh packet metadata needed by the Vercel/Neon ingest path;
- Wi-Fi HTTPS upload from the Heltec to `/api/ingest`;
- Wi-Fi OTA support on the Heltec V4.

Existing wire/database schemas are kept backward-compatible so the current dashboard and Neon ingestion do not break while names and modules are cleaned up.

## Useful DM commands

The current Heltec gateway build does **not yet expose custom gateway-control commands through ordinary Meshtastic text DMs**. It receives/forwards sensor traffic and uploads data, but the direct local HOBO BLE control feature is still pending.

These commands are reserved for that direct-HOBO feature once it is merged:

| Command | What it will do when direct HOBO BLE support is active |
|---|---|
| `LOGGER` | Show the selected/discovered HOBO model, MAC, BLE RSSI, interval, and lock state. |
| `READ` | Request an immediate fresh HOBO reading without disturbing the automatic record schedule. |
| `LOCK` | Save the currently selected HOBO BLE MAC so the gateway reconnects to that logger after reboot. |
| `UNLOCK` | Clear the saved HOBO assignment and return to logger discovery. |

Do not rely on those four custom text commands on the current Heltec build until direct HOBO BLE support has actually been merged and validated.

## Required next Heltec feature

The next Heltec firmware integration must add direct HOBO BLE support without removing any gateway behavior above:

1. scan for supported HOBO loggers;
2. identify the logger and expose its identity/status;
3. allow the selected logger to be locked by BLE MAC and persist that assignment across reboot;
4. automatically read confirmed new HOBO records according to the logger's own logging interval;
5. transmit the resulting reading over Meshtastic and upload it to Neon through the existing gateway path;
6. preserve direct-message commands including `READ` and `LOGGER`;
7. preserve the proven lock/unlock behavior from the universal HOBO field-node firmware.

## Architecture

```text
REMOTE FIELD SENSORS
  PIR / moisture / HOBO / battery / future sensors
              |
              v
        Meshtastic LoRa
              |
              v
CCA HELTEC SENSOR GATEWAY
  - accepts current packet formats
  - uploads to Vercel / Neon
  - future: direct local HOBO BLE scan/lock/read
              |
       +------+------+
       |             |
       v             v
 Meshtastic       HTTPS
   mesh         Vercel / Neon
```

The Heltec is the aggregation point. New sensor types should be added as independent parsers/adapters rather than by creating a new gateway branch for every sensor.

## GitHub build

Use the dedicated workflow:

```text
Actions -> Build CCA Heltec Sensor Gateway -> Run workflow
```

That workflow always builds the correct OLED target:

```text
heltec-v4 / esp32s3
```

The reusable build workflow injects `HOBO_HTTP_GATEWAY_INGEST_KEY` for both `heltec-v4` and `heltec-v4-tft`; the OLED build therefore receives the cloud credential during GitHub Actions builds.

## Wi-Fi OTA

Normal Meshtastic TCP API:

```text
4403
```

ESP32 Unified OTA loader:

```text
3232
```

The Heltec V4 uses the Meshtastic Unified OTA flow. Do not erase flash for routine updates; preserve NVS/configuration and the working OTA loader.

## Repository rules

1. `Heltec-Gateway-v2` is the current Heltec gateway development branch.
2. Do not create location-specific gateway branches such as Hidden Valley or sensor-specific gateway branches.
3. Location, logger identity, and sensor assignments belong in configuration/data, not branch names.
4. Keep existing sensor parsers working when adding a new one.
5. Do not rename or remove a wire/database schema until the ingest API and Neon migration are ready.
6. GitHub Actions is the normal build path; Wi-Fi OTA is the normal Heltec flash path.

See [`docs/CCA_HELTEC_SENSOR_GATEWAY.md`](docs/CCA_HELTEC_SENSOR_GATEWAY.md) for the operational specification and [`docs/BRANCH_MAP.md`](docs/BRANCH_MAP.md) for branch status.
