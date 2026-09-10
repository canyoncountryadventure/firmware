# CCA Heltec Sensor Gateway — Operational Specification

## Purpose

The Heltec V4 OLED is the permanent aggregation gateway for the CCA Meshtastic sensor network. It must participate normally in the mesh, read the local Home HOBO, accept automatic remote telemetry, trigger Fishlake reads, and forward accepted permanent-station data over Wi-Fi to Vercel/Neon with minimal unnecessary cloud wakeups and without making any remote radio a dependency for the rest of the network.

## Canonical branch and target

- Branch: `cca-heltec-sensor-gateway`
- Board: Heltec WiFi LoRa 32 V4 OLED
- PlatformIO environment: `heltec-v4`
- Architecture: `esp32s3`

Do not use `heltec-v4-tft` for the current physical gateway.

## Production station identities

| Station | Node ID | Node number | Mode |
|---|---|---:|---|
| Hidden Valley Repeater | `!b57d051f` | `3044869407` | automatic remote HOBO telemetry |
| Heltec Home | `!a35a4bf4` | `2740603892` | automatic local HOBO BLE |
| Fishlake Hightop | `!5e021e35` | `1577197109` | Heltec-triggered `READ` |
| It's a Swell Day | `!742ecff5` | `1949224949` | automatic remote HOBO telemetry |

`!55a55ce8 / 1436900584` is **not** Hidden Valley and must not be treated as the production Hidden Valley node.

## Station acquisition policy

### Hidden Valley — automatic remote

Hidden Valley's RAK4631/HOBO node acquires its own HOBO data and transmits standard Meshtastic environmental telemetry automatically. The Heltec listens for those packets; it does not need to send Hidden Valley a `READ` command for normal operation.

The deployed Hidden Valley logger is an MX2201 currently configured at a 3600-second logging interval. The field-node firmware follows the logger's actual write-pointer cadence rather than a separate radio timer.

Hidden Valley is **not** a cloud flush trigger.

### Home — automatic local and normal cloud trigger

The Heltec directly reads the selected Home HOBO over BLE. Home acquisition is automatic and does not depend on a remote Meshtastic packet.

Each new Home environmental reading enters the normal upload queue and becomes the **primary synchronized cloud batch trigger**. At that moment, the gateway drains any held permanent-remote environmental/device readings and sends Home plus those held readings in one HTTPS array request.

### It's a Swell Day — automatic remote

It's a Swell Day (`!742ecff5`, node `1949224949`) runs the RAK HOBO telemetry firmware and sends its own environmental telemetry over the mesh at the HOBO-derived logging cadence.

The Heltec receives Swell environmental and device/battery telemetry and holds it for the next Home-triggered cloud batch.

### Fishlake Hightop — Heltec-triggered

Fishlake is trigger/poll driven by `FishlakePollerModule`.

1. Heltec sends Meshtastic DM `READ` to Fishlake node `!5e021e35`.
2. Fishlake performs a fresh local HOBO read.
3. Fishlake returns the reading as a direct text reply.
4. Heltec accepts/parses the Fishlake reply.
5. Heltec places the normalized Fishlake reading in the same remote hold queue.
6. The next Home HOBO reading normally flushes it with the other held permanent-station jobs.
7. The normal Fishlake trigger interval is 60 minutes.

The Fishlake observation timestamp and radio metadata are preserved while the reading waits for the shared cloud flush.

## Channel policy

Production messaging-channel order is:

```text
channel 0: LayMesh
channel 1: LongFast
```

Logical Meshtastic channel index and the underlying LoRa RF slot/frequency are separate settings. Matching LayMesh/LongFast channel order and PSKs does not by itself prove two radios are tuned to the same RF frequency.

## Existing gateway inputs that must remain supported

### Environmental telemetry

Cloud upload is restricted to configured permanent remote stations: Hidden Valley, Fishlake Hightop, and It's a Swell Day. Unrelated public Meshtastic environmental telemetry must be discarded before HTTP work.

### Device telemetry

Standard Meshtastic device telemetry from configured permanent remote stations is retained, including battery level/voltage and other node-health fields. Device packets join the synchronized remote hold queue.

### Direct local HOBO BLE

The Heltec direct HOBO path supports the local Home station without replacing normal mesh behavior. Logger discovery/selection, live reads, and existing lock/state behavior must remain compatible with the current implementation.

### Fishlake text reply

The Fishlake poller accepts only the configured Fishlake node's `READ` reply, extracts the returned HOBO temperature/model information, and queues a normalized Fishlake record for the synchronized cloud flush.

### Legacy field sensor packets

Existing moisture/PIR and custom MX2001 packet parsing remain backward-compatible where already supported. These compatibility paths are separate from the permanent temperature-station batching policy.

## Cloud batching policy

Timed permanent-remote readings should share the Home station's normal cloud wake window instead of independently posting to the backend.

```text
Hidden Valley environment ------------> remote hold queue --+
It's a Swell Day environment ---------> remote hold queue --+
Permanent remote device telemetry ----> remote hold queue --+
Fishlake timed READ result -----------> remote hold queue --+
                                                           |
Home HOBO BLE environment -------------------------------+--> one HTTPS array POST --> Vercel --> Neon
```

Rules:

- Remote LoRa transmissions remain staggered according to their own acquisition schedules; only cloud upload is synchronized.
- Home environmental telemetry is the normal flush trigger.
- No remote station is ever a required trigger for another station.
- If no remote reading is pending, Home uploads by itself.
- The remote hold queue is capped at 48 readings.
- The Vercel ingest API must accept at least 49 readings so a full hold queue plus the Home trigger fits in one request.
- If the Home-triggered cloud POST fails, drained remote jobs are restored for retry.
- If the Home reading exhausts normal upload retries, it is preserved in the hold queue rather than discarded.
- Every held job keeps its original observation timestamp and radio metadata.
- Unrelated public Meshtastic environmental/device telemetry is dropped before HTTP and does not wake Vercel/Neon.

## Safety flush

Home is the normal clock, not a single point of failure. If held data has waited **70 minutes** without a successful Home-triggered flush, the Heltec drains the remote queue itself and uploads it as one safety batch.

A failed safety flush uses bounded retry backoff rather than tight-looping against Vercel/Neon.

This safety path exists specifically so a failed local BLE sensor, lost Home HOBO connection, or other Home acquisition problem cannot strand Hidden Valley, Swell, or Fishlake telemetry.

## Meshtastic coexistence requirement

All sensor behavior is additive. The Heltec must continue to operate as a normal Meshtastic node while the sensor gateway is active, including LoRa receive/transmit behavior and the configured TCP/API, Wi-Fi/web, MQTT, and OTA services enabled in the deployment.

## Extension rule for future sensors

Future permanent timed sensors should join the same Home-clock hold-and-flush path where practical rather than creating a separate HTTPS request per sensor. They must also remain protected by the time-based safety flush.

Do not create one firmware branch per sensor or deployment location.

## GitHub build path

Use:

```text
Actions -> Build CCA Heltec Sensor Gateway
```

The dedicated workflow builds `heltec-v4` on `esp32s3` and injects the HTTP ingest credential during the build.

## Wi-Fi OTA path

Normal updates use Meshtastic Unified OTA. Preserve NVS/configuration and the OTA loader; do not factory-erase the Heltec for routine firmware changes.

Ports:

- Meshtastic TCP API: `4403`
- Unified OTA loader: `3232`

Use the regular `firmware-heltec-v4-*.bin` from the successful GitHub artifact, not a factory image.

## Non-regression checklist

Every future Heltec build should verify:

- Meshtastic node boots and participates normally in the mesh.
- Home direct HOBO BLE reading remains automatic.
- Home environmental reading is the normal synchronized cloud flush trigger.
- Hidden Valley is `!b57d051f / 3044869407` and its environmental telemetry is accepted automatically.
- `!55a55ce8 / 1436900584` is not treated as Hidden Valley.
- It's a Swell Day environmental telemetry is received and held for the Home batch.
- Device/battery telemetry from Hidden Valley, Fishlake, and Swell remains accepted and held.
- Unrelated public environmental/device telemetry cannot create HTTP/Vercel/Neon work.
- Fishlake `READ` is initiated by `FishlakePollerModule`, replies from `!5e021e35` are parsed, and the resulting reading joins the remote hold queue.
- A 70-minute safety flush prevents Home failure from blocking remote data.
- Batched readings retain their original observation timestamps and RF metadata.
- Accepted readings upload to Vercel/Neon.
- Existing compatibility parsers remain functional where required.
- Cloud ingest credential is present in GitHub-built OLED artifact.
- Wi-Fi OTA still works.
