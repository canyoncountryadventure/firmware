# CCA Heltec Sensor Gateway

**Canonical branch:** `cca-heltec-sensor-gateway`  
**Hardware:** Heltec WiFi LoRa 32 V4 OLED  
**PlatformIO target:** `heltec-v4`  
**Cloud path:** Heltec -> Vercel ingest -> Neon PostgreSQL -> dashboard

This branch is the single production gateway branch for the CCA Meshtastic sensor network. The Heltec remains a normal Meshtastic radio/server while also reading the Home HOBO, receiving remote telemetry, polling Fishlake, and uploading accepted readings over Wi-Fi.

## Production station identities

| Station | Node ID | Node number | Acquisition |
|---|---:|---:|---|
| **Hidden Valley Repeater** | `!b57d051f` | `3044869407` | automatic remote RAK4631 + HOBO MX2201 |
| **Heltec Home** | `!a35a4bf4` | `2740603892` | automatic local HOBO BLE |
| **Fishlake Hightop** | `!5e021e35` | `1577197109` | Heltec-triggered `READ` |
| **It's a Swell Day** | `!742ecff5` | `1949224949` | automatic remote RAK4631 + HOBO |

**Important:** `!55a55ce8 / 1436900584` is not Hidden Valley and must not be used as the production Hidden Valley identity.

## Acquisition modes

Hidden Valley and It's a Swell Day read their own HOBO loggers and send Meshtastic telemetry. Home is read directly by the Heltec over BLE. Fishlake is polled by the Heltec with a direct `READ` command every 60 minutes.

The remote RAK HOBO production firmware follows the logger's actual HOBO interval. Hidden Valley is currently configured for a 3600-second HOBO interval.

## Cloud batching — Home is the clock

The local **Heltec Home HOBO environmental reading is the normal cloud flush trigger**. Remote permanent-station environmental/device packets are held on the Heltec until the next Home reading, then submitted with Home in one HTTPS array request.

```text
Hidden Valley telemetry ---------> held remote queue --+
It's a Swell Day telemetry ------> held remote queue --+
Fishlake timed READ result ------> held remote queue --+
remote device/battery telemetry -> held remote queue --+
                                                        |
Home HOBO BLE reading ---------------------------------+--> one HTTPS batch
                                                             |
                                                             v
                                                        Vercel /api/ingest
                                                             |
                                                             v
                                                            Neon
```

This design deliberately avoids making any remote radio a dependency for the rest of the network.

### Failure protection

- A missing Hidden Valley packet cannot block Home, Swell, or Fishlake.
- A missing Swell/Fishlake packet cannot block any other station.
- If the local Home HOBO trigger itself stops, the Heltec performs a **70-minute safety flush** of held remote data.
- Held queue capacity is 48 readings.
- Failed Home batches restore remote jobs for retry.
- A Home reading that exhausts normal upload retries is preserved in the held queue for the safety-flush path.
- Observation timestamps and RF metadata are preserved while readings wait for the cloud batch.
- Unconfigured/public Meshtastic environmental/device telemetry is discarded before creating cloud work.

## Gateway responsibilities

The current branch preserves:

- normal Meshtastic LoRa radio/client operation;
- Meshtastic TCP/API and web services;
- Wi-Fi connectivity and HTTPS upload;
- direct local HOBO BLE reading for Home;
- Hidden Valley automatic environmental and device/battery telemetry;
- It's a Swell Day automatic environmental and device/battery telemetry;
- Fishlake remote `READ` polling and reply parsing;
- existing moisture/PIR and MX2001 compatibility paths;
- Wi-Fi Unified OTA on Heltec V4.

## Channel policy

Production station messaging channels are:

```text
channel 0: LayMesh
channel 1: LongFast
```

Channel index and LoRa RF slot/frequency are different settings. Matching channel names/PSKs alone does not prove two radios are on the same RF frequency.

## Architecture

```text
HIDDEN VALLEY (automatic) -------- TELEMETRY_APP -----+
SWELL (automatic) ---------------- TELEMETRY_APP -----+--> HELTEC HOME
FISHLAKE <--------- DM READ / text HOBO reply --------+
HOME HOBO ------------------------------ BLE ---------+
                                                        |
                                                        +--> Wi-Fi HTTPS --> Vercel --> Neon
```

## GitHub build

Use the dedicated workflow:

```text
Actions -> Build CCA Heltec Sensor Gateway
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

Use the regular `firmware-heltec-v4-*.bin` from the GitHub build artifact for routine OTA updates. Do not use the factory image and do not erase NVS/configuration.

## Repository rules

1. `cca-heltec-sensor-gateway` is the single current Heltec gateway branch.
2. Hidden Valley is `!b57d051f / 3044869407`.
3. Home is the normal cloud batch trigger; no remote station may be a required trigger.
4. A time-based fallback must remain so Home failure cannot strand remote telemetry.
5. Hidden Valley, Home, and Swell remain automatic acquisition paths; Fishlake remains Heltec-polled unless intentionally redesigned.
6. Do not create location-specific Heltec firmware branches.
7. Keep ordinary Meshtastic radio/server behavior working while sensor features are added.
8. Keep packet/database formats backward-compatible unless the backend is migrated at the same time.
9. GitHub Actions is the normal build path; Wi-Fi OTA is the normal Heltec update path.

See [`docs/CCA_HELTEC_SENSOR_GATEWAY.md`](docs/CCA_HELTEC_SENSOR_GATEWAY.md) for the operational specification.
