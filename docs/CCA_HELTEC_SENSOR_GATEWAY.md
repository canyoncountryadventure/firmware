# CCA Heltec Sensor Gateway — Operational Specification

## Purpose

The Heltec V4 OLED is the permanent aggregation gateway for the CCA Meshtastic sensor network. It must accept data from multiple field-node sensor types, preserve existing packet support, forward data over Wi-Fi to the Vercel/Neon backend, and continue to participate normally in the Meshtastic mesh.

## Canonical branch and target

- Branch: `cca-heltec-sensor-gateway`
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

The existing private application record includes:

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

## Direct HOBO BLE feature to add to the Heltec

The direct HOBO reader on the Heltec must be ported from the proven universal HOBO implementation without replacing or disabling the mesh gateway.

Required behavior:

1. BLE discovery scans for supported HOBO MX2001, MX2201, and MX2203 loggers.
2. The current candidate logger can be inspected remotely.
3. `LOCK` stores the selected logger BLE MAC in persistent storage.
4. A locked Heltec reconnects only to that logger after reboot.
5. `UNLOCK` clears the assignment and returns to discovery.
6. `LOGGER` reports logger model, MAC, BLE RSSI, logger logging interval, and lock state.
7. `READ` performs an immediate fresh reading without corrupting or advancing the automatic schedule.
8. Automatic reads follow confirmed logger records/write-pointer advancement rather than an unrelated free-running timer.
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

`Actions -> Build CCA Heltec Sensor Gateway -> Run workflow`

The dedicated workflow builds only `heltec-v4` on `esp32s3` and uses the repository secret `HOBO_HTTP_GATEWAY_INGEST_KEY` during the build.

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
- Direct HOBO scan/lock/read/automatic telemetry works once that feature is merged.
- `READ`, `LOGGER`, `LOCK`, and `UNLOCK` remain functional once direct HOBO support is merged.
