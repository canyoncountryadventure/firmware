# CCA Heltec Sensor Gateway — Operational Specification

## Purpose

The Heltec V4 OLED is the permanent aggregation gateway for the CCA Meshtastic sensor network. It must participate normally in the mesh, read the local Home HOBO, accept automatic remote telemetry, trigger Fishlake reads, and forward accepted permanent-station data over Wi-Fi to Vercel/Neon with minimal unnecessary cloud wakeups.

## Canonical branch and target

- Branch: `cca-heltec-sensor-gateway`
- Board: Heltec WiFi LoRa 32 V4 OLED
- PlatformIO environment: `heltec-v4`
- Architecture: `esp32s3`

Do not use `heltec-v4-tft` for the current physical gateway.

## Station acquisition policy

This policy is intentional and should be preserved during future firmware changes.

### Hidden Valley — automatic / cloud batch trigger

Hidden Valley's remote RAK/HOBO node acquires its own HOBO data and transmits standard Meshtastic environmental telemetry automatically. Heltec listens for those packets; it does not need to send Hidden Valley a `READ` command for normal operation.

Hidden Valley environmental temperature is the normal cloud flush trigger for held permanent-station readings.

### Home — automatic

Heltec directly reads the selected Home HOBO over BLE. Home acquisition is automatic and does not depend on a remote Meshtastic request.

Pending Home environmental readings are held locally. When Hidden Valley environmental telemetry arrives, the gateway submits Hidden Valley plus the other held permanent-station readings in one HTTPS array request when possible. Original observation times are retained.

### It's a Swell Day — automatic remote

It's a Swell Day (`!742ecff5`, node `1949224949`) runs the RAK HOBO telemetry firmware and sends its own environmental telemetry over the mesh at the HOBO-derived logging cadence.

The Heltec receives Swell telemetry normally, but does **not** open a separate HTTPS request for each Swell environmental or device/battery packet. Swell jobs are held for the next synchronized Hidden Valley cloud flush.

### Fishlake Hightop — Heltec-triggered

Fishlake is **not** treated as a free-running automatic remote telemetry source in this gateway policy. The Heltec owns the trigger schedule through `FishlakePollerModule`.

Current path:

1. Heltec sends Meshtastic DM `READ` to Fishlake node `!5e021e35`.
2. Fishlake performs a fresh local HOBO read.
3. Fishlake returns the reading as a direct text reply.
4. Heltec accepts/parses the Fishlake reply.
5. Heltec places the normalized Fishlake reading in the synchronized cloud hold queue.
6. The next Hidden Valley environmental packet flushes it with the other held permanent-station jobs.
7. The normal Fishlake trigger interval is 60 minutes.

The Fishlake observation timestamp and radio metadata are preserved while the reading waits for the shared cloud flush.

## Existing gateway inputs that must remain supported

### Environmental telemetry

Standard Meshtastic environmental telemetry remains available to the gateway, but cloud upload is restricted to configured permanent remote stations: Hidden Valley, Fishlake Hightop, and It's a Swell Day. Unrelated public Meshtastic environmental telemetry must be discarded before the HTTP queue so it cannot consume Vercel or Neon resources.

### Device telemetry

Standard Meshtastic device telemetry from the configured permanent remote stations is retained, including battery level/voltage and other node-health fields. Device packets are held for the synchronized cloud flush rather than independently waking Vercel/Neon.

### Direct local HOBO BLE

The Heltec direct HOBO path supports the local Home station without replacing normal mesh behavior. Logger discovery/selection, live reads, and existing lock/state behavior must remain compatible with the current implementation.

### Fishlake text reply

The Fishlake poller accepts only the configured Fishlake node's `READ` reply, extracts the returned HOBO temperature/model information, and queues a normalized Fishlake record for the synchronized cloud flush.

### Legacy field sensor packets

Existing sandstone moisture/PIR and custom MX2001 packet parsing should remain backward-compatible where already supported. These compatibility paths are separate from the permanent temperature-station batching policy.

## Cloud batching policy

Timed permanent-station readings should share one normal HTTPS/Vercel/Neon wake window instead of independently posting to the backend.

```text
Home BLE reading ----------------------> held queue --+
It's a Swell Day environment ----------> held queue --+
Permanent-station device telemetry ----> held queue --+
Fishlake timed READ result ------------> held queue --+
                                                     |
Hidden Valley environment ---------------------------+--> one HTTPS array POST --> Vercel --> Neon
```

Rules:

- Remote LoRa transmissions are **not** deliberately synchronized to the same instant. They remain staggered according to their own acquisition schedule to avoid increasing RF collisions.
- Synchronization occurs at the **cloud upload layer**.
- Hidden Valley environmental telemetry is the normal flush trigger.
- Hidden Valley is never blocked waiting for another station.
- If no held reading exists, Hidden Valley uploads by itself.
- If a batched cloud POST fails, held jobs are returned to the hold queue for retry.
- Every held job keeps its original observation timestamp and radio metadata.
- The Vercel ingest API accepts up to 24 readings in one request; the gateway hold queue is capped at 20 plus the Hidden Valley trigger, keeping a normal flush within that limit.
- Unrelated public Meshtastic environmental/device telemetry is dropped before HTTP and does not wake Vercel/Neon.

## Meshtastic coexistence requirement

All sensor behavior is additive. The Heltec must continue to operate as a normal Meshtastic node while the sensor gateway is active, including LoRa receive/transmit/relay behavior and the configured TCP/API, Wi-Fi/web, MQTT, and OTA services that are enabled in the deployment.

## Extension rule for future sensors

Future permanent timed sensors should join the same hold-and-flush cloud path where practical rather than creating a separate HTTPS request per sensor.

```text
sensor packet / local sensor
        |
        v
sensor-specific decoder
        |
        v
normalized gateway job
        |
   permanent timed? ---- yes ---> synchronized hold queue
        |                              |
        no                             v
        |                      Hidden Valley flush
        v                              |
 immediate/special path                v
                                one HTTPS array POST
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
- Hidden Valley environmental telemetry is received automatically and remains the normal cloud flush trigger.
- It's a Swell Day environmental telemetry is received and held for the synchronized cloud flush.
- Device/battery telemetry from Hidden Valley, Fishlake, and Swell remains accepted and held for the synchronized cloud flush.
- Unrelated public environmental/device telemetry cannot create HTTP/Vercel/Neon work.
- Fishlake `READ` is initiated by `FishlakePollerModule`, replies from `!5e021e35` are parsed, and the resulting reading joins the synchronized cloud hold queue.
- Fishlake remains trigger/poll driven rather than being silently changed to free-running automatic acquisition in the gateway policy.
- Batched readings retain their original observation timestamps.
- Accepted readings upload to Vercel/Neon.
- Existing compatibility parsers remain functional where required.
- Cloud ingest credential is present in GitHub-built OLED artifact.
- Wi-Fi OTA still works.
