# CCA Heltec Sensor Gateway V5 — Operational Specification

## Purpose

The Heltec WiFi LoRa 32 **hardware V4 OLED running gateway firmware V5** is the permanent aggregation gateway for the CCA Meshtastic sensor network. It must accept data from multiple field-node sensor types, preserve existing packet support, forward data over Wi-Fi to the Vercel/Neon backend, and continue to participate normally in the Meshtastic mesh.

## Canonical branch and target

- Current firmware branch: `Heltec-Gateway-v5` (previous `Heltec-Gateway-v4` retained)
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

`Actions -> Build Heltec Gateway V5 (dated release) -> Run workflow`

The dedicated V5 workflow builds only `heltec-v4` on `esp32s3`, uses the repository secret `HOBO_HTTP_GATEWAY_INGEST_KEY`, and after successful compilation commits date-named BIN/ZIP plus SHA-256 hashes to `downloads/Heltec-Gateway-v5`. These files are linked in the branch README. Firmware V5 is not a Heltec hardware revision. The matching RAK V6 field node must use RAK UF2, never this ESP32-S3 BIN.

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

## Heltec V5 compatibility acceptance

- Verify legacy 19-byte MX2001 `MX` V1 stage and temperature are divided by 10, not 100.
- Verify 21-byte `MX` V2 uses stage signed int32 LE /100, temperature signed int16 LE /100, raw thermometer int16, six MAC bytes and RSSI at byte 20. Reject mismatched version/length.
- Verify cloud body forwards stage, F and C with three decimal places available and the API/database do not round the incoming stage back to tenths.
- Verify the gateway still forwards environmental, device battery percentage/voltage, water distance, rock moisture/PIR and soil packets.
- Verify retained watchdog (`DIAG`, `WATCHDOG`, 12-hour maintenance, 90-second watchdog), OTA, local HOBO logger lock/scan/reading and mesh/HTTPS coexistence.
- Inspect dated firmware links, source commit and SHA-256 in [the Heltec V5 README](../README.md). CI compilation alone does not validate physical BLE, radio or cloud ingestion.
