# CCA Heltec Sensor Gateway

> **Canonical production branch:** `cca-heltec-sensor-gateway`
>
> **Hardware:** Heltec WiFi LoRa 32 V4 OLED
>
> **PlatformIO target:** `heltec-v4`
>
> **Cloud path:** Heltec → Vercel ingest → Neon PostgreSQL → dashboard

This branch is the single production Heltec gateway line for the CCA Meshtastic sensor network. The Heltec remains a normal Meshtastic radio/server while also reading the Home HOBO, receiving remote telemetry, polling Fishlake, batching accepted observations, and uploading them over Wi-Fi.

## Branch policy

Use this branch for production gateway work.

Historical branches such as:

```text
heltec-home-http-gateway
heltec-home-http-gateway-hidden-valley
heltec-home-http-gateway-rock
```

are legacy development snapshots and should not be used as current deployment sources. New location-specific Heltec branches should not be created unless a temporary isolated test genuinely requires one.

The experimental water-alert gateway remains separate on:

```text
fucking-around-heltec
```

## Production station identities

| Station | Node ID | Node number | Acquisition |
|---|---:|---:|---|
| **Hidden Valley Repeater** | `!b57d051f` | `3044869407` | automatic remote RAK4631 + HOBO MX2201 |
| **Heltec Home** | `!a35a4bf4` | `2740603892` | automatic local HOBO BLE |
| **Fishlake Hightop** | `!5e021e35` | `1577197109` | Heltec-triggered `READ` |
| **It's a Swell Day** | `!742ecff5` | `1949224949` | automatic remote RAK4631 + HOBO |

**Do not use** `!55a55ce8 / 1436900584` as the production Hidden Valley identity.

## Acquisition modes

- **Hidden Valley:** its field node reads its own HOBO and transmits automatic Meshtastic telemetry.
- **It's a Swell Day:** its field node reads its own HOBO and transmits automatic telemetry.
- **Home:** the Heltec reads the local HOBO directly over BLE.
- **Fishlake:** the Heltec sends a direct `READ` request on its polling schedule and parses the reply.

Remote RAK HOBO production firmware follows the logger's actual HOBO interval. Hidden Valley is currently configured for a 3600-second logger interval.

## Cloud batching — Home is the normal clock

Remote permanent-station observations are held on the Heltec until the next Home HOBO observation, then uploaded with Home in one HTTPS array request.

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

No remote station is allowed to become a dependency for the rest of the network.

### Failure protection

- Missing Hidden Valley data cannot block Home, Swell, or Fishlake.
- Missing Swell/Fishlake data cannot block any other station.
- If the Home HOBO trigger stops, the gateway performs a **70-minute safety flush** of held remote data.
- Held queue capacity is 48 readings.
- Failed Home batches restore remote jobs for retry.
- A Home reading that exhausts normal upload retries is preserved for the safety-flush path.
- Observation timestamps and RF metadata are preserved while data waits in the queue.
- Unconfigured/public Meshtastic environmental/device telemetry is discarded before cloud work is created.

## Gateway responsibilities

The production branch preserves:

- normal Meshtastic LoRa radio/client operation;
- Meshtastic TCP/API and web services;
- Wi-Fi connectivity and HTTPS upload;
- direct local HOBO BLE reading for Home;
- Hidden Valley automatic environmental and device/battery telemetry;
- It's a Swell Day automatic environmental and device/battery telemetry;
- Fishlake remote `READ` polling and reply parsing;
- moisture/PIR and MX2001 compatibility paths already intentionally supported;
- Wi-Fi Unified OTA on Heltec V4.

## Channel policy

Production station messaging channels are:

```text
channel 0: LayMesh
channel 1: LongFast
```

Channel index and LoRa RF slot/frequency are different settings. Matching names/PSKs alone does not prove two radios are on the same RF frequency.

## Architecture

```text
HIDDEN VALLEY (automatic) -------- TELEMETRY_APP -----+
SWELL (automatic) ---------------- TELEMETRY_APP -----+--> HELTEC HOME
FISHLAKE <--------- DM READ / text HOBO reply --------+
HOME HOBO ------------------------------ BLE ---------+
                                                        |
                                                        +--> Wi-Fi HTTPS --> Vercel --> Neon
```

## Build

Use the dedicated GitHub workflow:

```text
Actions → Build CCA Heltec Sensor Gateway
```

It builds:

```text
heltec-v4 / esp32s3
```

The workflow injects `HOBO_HTTP_GATEWAY_INGEST_KEY` at build time. Do not commit the real ingest key into source.

## Wi-Fi OTA

Normal Meshtastic TCP API:

```text
4403
```

ESP32 Unified OTA loader:

```text
3232
```

For routine OTA updates, use the regular `firmware-heltec-v4-*.bin` from the build artifact. Do not use a factory image and do not erase NVS/configuration unless an intentional full reset is required.

## Production rules

1. `cca-heltec-sensor-gateway` is the single current production Heltec gateway branch.
2. Hidden Valley is `!b57d051f / 3044869407`.
3. Home is the normal cloud batch trigger; no remote station may be a required trigger.
4. Preserve a time-based fallback so Home failure cannot strand remote telemetry.
5. Hidden Valley, Home, and Swell remain automatic acquisition paths; Fishlake remains Heltec-polled unless intentionally redesigned.
6. Do not create location-specific production gateway branches.
7. Keep ordinary Meshtastic radio/server behavior working while sensor features are added.
8. Keep packet/database formats backward-compatible unless the backend is migrated at the same time.
9. GitHub Actions is the normal build path; Wi-Fi OTA is the normal Heltec update path.
10. Experimental water-alert work stays on `fucking-around-heltec` until intentionally promoted.
11. Preserve retired gateway milestones with tags rather than leaving obsolete branches that look deployable.

See [`docs/CCA_HELTEC_SENSOR_GATEWAY.md`](docs/CCA_HELTEC_SENSOR_GATEWAY.md) for the detailed operational specification.
