# CCA Heltec Sensor Gateway V6 — Operational Specification

## Purpose

The Heltec WiFi LoRa 32 **hardware V4 OLED running gateway firmware V6** is the permanent aggregation gateway for the CCA Meshtastic sensor network. It must accept data from multiple field-node sensor types, preserve existing packet support, forward data over Wi-Fi to the Vercel/Neon backend, and continue to participate normally in the Meshtastic mesh.

## Canonical branch and target

- Current firmware branch: `Heltec-Gateway-v6` (V5/V4 retained for rollback)
- Board: Heltec WiFi LoRa 32 V4 OLED
- PlatformIO environment: `heltec-v4`
- Architecture: `esp32s3`

Do not use `heltec-v4-tft` for the current physical gateway.

## Existing gateway inputs that must remain supported

### Sandstone moisture + PIR

A field node can send the existing 16-byte private application packet containing:

- current motion state;
- cumulative motion count;
- moisture/sensor ADC;
- sensor voltage;
- battery voltage;
- battery percentage.

The current wire/database format remains backward-compatible until the server-side schema is intentionally migrated.

### HOBO MX2001

The gateway accepts both old `MX` V1 19-byte records (tenths of a foot and Fahrenheit degree) and new `MX` V2 21-byte records (signed 32-bit stage and 16-bit temperature in hundredths), preserving the prior node deployments. Both versions include:

- water level/stage;
- temperature;
- raw temperature value;
- logger BLE MAC;
- sequence;
- BLE RSSI;
- Meshtastic/LoRa metadata.

### Environmental telemetry

Standard Meshtastic environmental telemetry is accepted and forwarded. This currently carries HOBO MX2201 temperature and can carry other environmental sensors later.

### Device telemetry

Standard Meshtastic device telemetry is accepted and forwarded, including:

- battery level;
- voltage;
- channel utilization;
- transmit airtime utilization;
- uptime.

This preserves the Hidden Valley battery-monitoring path and makes the gateway useful for future remote-node health monitoring.

## Direct HOBO BLE support

The direct HOBO reader is active on the Heltec Gateway V5 and coexists with ordinary mesh forwarding and HTTP/Neon ingestion.

Current behavior:

1. BLE discovery scans for supported HOBO MX2001, MX2201, and MX2203 loggers.
2. The current candidate logger can be inspected remotely.
3. `LOCK` stores the selected logger BLE MAC in persistent storage.
4. A locked Heltec reconnects only to that logger after reboot.
5. `UNLOCK` clears the assignment and returns to discovery.
6. `LOGGER` reports logger model, MAC, BLE RSSI, logger logging interval, and lock state.
7. `READ` performs an immediate fresh reading without corrupting or advancing the automatic schedule.
8. Automatic reads follow confirmed logger records/write-pointer advancement rather than an unrelated free-running timer. MX2001 mesh packets produced locally now use the new 21-byte V2 hundredths format.
9. Each successful automatic reading is sent over Meshtastic and also enters the existing Heltec HTTP/Neon upload path.
10. BLE activity must not disable ordinary Meshtastic receive/relay/gateway behavior.

## V6 CC01 DM fallback and PKI reply repair

The existing V5 Heltec Gateway remains the base for V6: no change to the mesh, cloud HTTP, watchdog, BLE/HOBO local reader, node allowlist, all original private packet decoders, stage calibration or RAK field firmware. V6's additive fallback targets CC01 `!65165646` via Heltec LongFast local channel index 1.

- At about 2 minutes after boot, then every 30 minutes: send `WATER READ` by reliable LoRa DM. If no valid calibrated reply enters the upload queue, retry every 120 seconds with no retry-count limit.
- After the first water reply (or first 120-second timeout), wait at least 3 seconds and send `READ` by reliable LoRa DM. If no valid `MX2001` reply enters the upload queue, retry every 120 seconds with no retry-count limit. Each command has its own pending flag and timestamp: neither success nor failure can cancel the other. Preserve at least 3 seconds between any outbound CC01 DMs. At each 30-minute cycle re-arm only types that completed, leaving missing readings' retries active.
- Only parse CC01 `TEXT_MESSAGE_APP` DMs addressed to Heltec. Accept complete sensor replies in every polling state, including manual and late replies; only a matching reply advances the scheduled poll. **V6 accepts authenticated PKI replies on Meshtastic's decoded channel index 0, plus non-PKI replies on LongFast channel index 1.** The initial V6 build rejected PKI replies by checking only index 1. Ignore text that is not a complete valid sensor response.
- Convert successful `WATER READ` into existing `water_distance` job with precise raw mm, stage mm/ft, `distance_valid`, `stage_calibrated`, `sensor_type` MB7388, plus `payload.source = "heltec_dm"`. Do not invent missing stages or sequence numbers.
- Convert successful `MX2001` reply into existing `mx2001` job with stage, F/C, logger MAC and BLE RSSI, plus `payload.source = "heltec_dm"`. READ text reports hundredths of a foot and tenths F; do not invent original raw temperature or sequence.
- Use current V5 upload queue and credential, `Heltec -> Vercel /api/ingest -> Neon moab_watersheds.creek_observations`. Retain the DM reply's LoRa packet ID, sending station identity and radio metadata. Failed HTTP uploads use existing bounded retries.
- Do not suppress, deduplicate by measurement time, intercept or disable any automatic `DS` and `MX` private packets or standard battery/environment packets. Duplicate auto and DM measurements are permitted by request.
- V5 over-the-air reflash restores auto-only when the mesh is stable. Both V5 and V6 preserve the configuration across routine OTA.
- Acceptance requires **physical verification** of both DM replies uploaded to Neon, plus independent automatic packet ingestion; source compilation alone is insufficient.

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

Do not create one firmware branch per sensor or per deployment location.

## GitHub build path

Use:

`Actions -> Build Heltec Gateway V6 (dated release) -> Run workflow`

The dedicated V6 workflow builds only `heltec-v4` on `esp32s3`, uses the repository secret `HOBO_HTTP_GATEWAY_INGEST_KEY`, and after successful compilation commits date-named BIN/ZIP plus SHA-256 hashes to `downloads/Heltec-Gateway-v6`. These files are linked in the branch README. Firmware V6 is not a Heltec hardware revision. The matching RAK V6 field node must use RAK UF2, never this ESP32-S3 BIN.

## Wi-Fi OTA path

Normal updates should be performed over Wi-Fi using the Meshtastic Unified OTA path. Preserve NVS/configuration and the OTA loader; do not perform a factory erase for routine firmware changes.

Ports:

- Meshtastic TCP API: `4403`
- Unified OTA loader: `3232`

## Non-regression checklist

Every future Heltec build should be checked against this list:

- Meshtastic node boots and joins the mesh.
- Receives sandstone moisture/PIR packets.
- Receives HOBO MX2001 packets.
- Receives standard environmental telemetry.
- Receives standard device/battery telemetry.
- Uploads accepted packets to Vercel/Neon.
- Cloud ingest credential is present in GitHub-built OLED artifact.
- Wi-Fi OTA still works.
- Direct HOBO scan/lock/read/automatic telemetry works.
- `READ`, `LOGGER`, `LOCK`, and `UNLOCK` remain functional.

## Heltec V6 compatibility acceptance

- Verify a valid PKI-encrypted CC01 reply on decoded channel 0 is accepted by the gateway, and non-PKI traffic on channel 0 or unrelated nodes is rejected.
- Verify the serial log differentiates a send attempt from a confirmed response and reports malformed DM reply format.
- Verify legacy 19-byte MX2001 `MX` V1 stage and temperature are divided by 10, not 100.
- Verify 21-byte `MX` V2 uses stage signed int32 LE /100, temperature signed int16 LE /100, raw thermometer int16, six MAC bytes and RSSI at byte 20. Reject mismatched version/length.
- Verify cloud body forwards stage, F and C with three decimal places available and the API/database do not round the incoming stage back to tenths.
- Verify the gateway still forwards environmental, device battery percentage/voltage, water distance, rock moisture/PIR and soil packets.
- Verify retained watchdog (`DIAG`, `WATCHDOG`, 12-hour maintenance, 90-second watchdog), OTA, local HOBO logger lock/scan/reading and mesh/HTTPS coexistence.
- Inspect dated firmware links, source commit and SHA-256 in [the Heltec V6 README](../README.md). CI compilation alone does not validate physical BLE, radio or cloud ingestion.


## Wi-Fi gateway diagnostics (October 19 V6 revision)

`GET /json/gateway` on the Heltec's LAN HTTP or HTTPS server returns non-secret gateway status. Inspect `cc01.replies`, `parsed`, `queued`, `upload_attempts`, `uploaded`, and `last_http_status` to identify the first failed boundary. `filtered_channel`, `rejected_format`, `duplicates`, `queue_full`, and `dropped_upload` identify discarded packets. Packet IDs tie the last reply and last HTTP attempt to the Wi-Fi Meshtastic capture. Counts reset at reboot. No polling or upload is triggered by the endpoint.

Complete CC01 sensor replies must be queued in IDLE, WAIT_WATER, SEND_HOBO and WAIT_HOBO. A late water reply while waiting for HOBO must not cancel that wait, and a manual HOBO reply while waiting for water must not cancel the water wait. Keep sender, destination, decoded-channel/PKI checks and packet-ID duplicate protection intact.

## October 19, 2026: Dual-command retry repair

The poller tracks `water_pending` and `hobo_pending` independently with separate timeout clocks and cumulative `water_retries` / `hobo_retries` counters. The first `READ` does not block waiting forever for the water sensor: it starts after the first valid water response or the original two-minute water timeout, while WATER READ continues retrying in parallel. Either type stops only after successful upload-queue admission; malformed/unavailable messages or queue saturation do not satisfy a pending request. Existing cloud routing, packet authorization, automatic DS/MX reception and all other station paths remain unchanged. The `/json/gateway` firmware marker is `v6-cc01-dual-retry-2026-10-19`.
