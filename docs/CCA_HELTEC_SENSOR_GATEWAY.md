# CCA Heltec Sensor Gateway — Operational Specification

## Purpose

The Heltec V4 OLED is the permanent aggregation gateway for the CCA Meshtastic sensor network. It must participate normally in the mesh, read the local Home HOBO, accept automatic remote telemetry, trigger Fishlake reads, and forward accepted data over Wi-Fi to Vercel/Neon.

## Canonical branch and target

- Branch: `cca-heltec-sensor-gateway`
- Board: Heltec WiFi LoRa 32 V4 OLED
- PlatformIO environment: `heltec-v4`
- Architecture: `esp32s3`

Do not use `heltec-v4-tft` for the current physical gateway.

## Station acquisition policy

This policy is intentional and should be preserved during future firmware changes.

### Hidden Valley — automatic

Hidden Valley's remote RAK/HOBO node acquires its own HOBO data and transmits standard Meshtastic environmental telemetry automatically. Heltec listens for those packets; it does not need to send Hidden Valley a `READ` command for normal operation.

Hidden Valley environmental temperature also acts as the cloud batch trigger for held Home temperature readings.

### Home — automatic

Heltec directly reads the selected Home HOBO over BLE. Home acquisition is automatic and does not depend on a remote Meshtastic request.

Pending Home environmental readings are held locally. When Hidden Valley environmental telemetry arrives, the gateway submits Hidden Valley plus pending Home readings in one HTTPS array request when possible. Original Home observation times are retained.

### Fishlake Hightop — Heltec-triggered

Fishlake is **not** treated as a free-running automatic remote telemetry source. The Heltec owns the trigger schedule through `FishlakePollerModule`.

Current path:

1. Heltec sends Meshtastic DM `READ` to Fishlake node `!5e021e35`.
2. Fishlake performs a fresh local HOBO read.
3. Fishlake returns the reading as a direct text reply.
4. Heltec accepts/parses the Fishlake reply.
5. Heltec normalizes and uploads it as station `Fishlake Hightop`.
6. The normal trigger interval is 60 minutes.

**Summary: Fishlake is Heltec-triggered; Hidden Valley and Home are automatic.**

## Existing gateway inputs that must remain supported

### Environmental telemetry

Standard Meshtastic environmental telemetry is accepted and forwarded. This is the preferred automatic remote HOBO transport and is used by Hidden Valley.

### Device telemetry

Standard Meshtastic device telemetry is accepted and forwarded, including battery level/voltage and other node-health fields. This preserves Hidden Valley battery monitoring.

### Direct local HOBO BLE

The Heltec direct HOBO path supports the local Home station without replacing normal mesh behavior. Logger discovery/selection, live reads, and existing lock/state behavior must remain compatible with the current implementation.

### Fishlake text reply

The Fishlake poller accepts the configured Fishlake node's `READ` reply, extracts the returned HOBO temperature/model information, and uploads a normalized Fishlake record.

### Legacy field sensor packets

Existing sandstone moisture/PIR and custom MX2001 packet parsing should remain backward-compatible where already supported. These compatibility paths do not change the three-station acquisition policy above.

## Cloud batching policy

Home should not independently wake the cloud backend for each local HOBO read. Pending Home environmental jobs remain held until a Hidden Valley environmental packet arrives.

```text
Hidden Valley automatic telemetry ----+
                                      |
Home automatic BLE -> held queue -----+--> one HTTPS array POST -> Vercel -> Neon
```

Rules:

- Hidden Valley is never blocked waiting for Home.
- If Home has no pending reading, Hidden Valley uploads by itself.
- If a batched cloud POST fails, held Home readings are returned to the hold queue for retry.
- Home keeps the timestamp from the original BLE observation.
- Fishlake is a separate Heltec-triggered request/reply path and is not the Home batch trigger.

## Meshtastic coexistence requirement

All sensor behavior is additive. The Heltec must continue to operate as a normal Meshtastic node while the sensor gateway is active, including LoRa receive/transmit/relay behavior and the configured TCP/API, Wi-Fi/web, MQTT, and OTA services that are enabled in the deployment.

## Extension rule for future sensors

Future sensor integrations should use independent adapters/parsers with a common gateway output path:

```text
sensor packet / local sensor
        |
        v
sensor-specific decoder
        |
        v
normalized gateway job
        |
   +----+----+
   |         |
   v         v
Mesh TX   HTTP/Neon
```

Do not create one firmware branch per sensor or deployment location.

## GitHub build path

Use:

```text
Actions -> Build CCA Heltec Sensor Gateway -> Run workflow
```

The dedicated workflow builds `heltec-v4` on `esp32s3` and injects the HTTP ingest credential during the build.

## Wi-Fi OTA path

Normal updates should use Meshtastic Unified OTA. Preserve NVS/configuration and the OTA loader; do not factory-erase the Heltec for routine firmware changes.

Ports:

- Meshtastic TCP API: `4403`
- Unified OTA loader: `3232`

## Non-regression checklist

Every future Heltec build should verify:

- Meshtastic node boots and participates normally in the mesh.
- Home direct HOBO BLE reading remains automatic.
- Hidden Valley environmental telemetry is received automatically.
- Hidden Valley device/battery telemetry remains accepted.
- Hidden Valley arrival flushes pending Home temperature in the intended batch path.
- Fishlake `READ` is initiated by `FishlakePollerModule` and replies from `!5e021e35` are parsed.
- Fishlake remains trigger/poll driven rather than being silently changed to free-running automatic acquisition.
- Accepted readings upload to Vercel/Neon.
- Existing compatibility parsers remain functional where required.
- Cloud ingest credential is present in GitHub-built OLED artifact.
- Wi-Fi OTA still works.
