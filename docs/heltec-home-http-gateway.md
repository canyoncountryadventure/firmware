# Heltec Home HTTP Gateway

This branch turns the Heltec WiFi LoRa 32 V4 + TFT into a direct Meshtastic-to-cloud gateway for HOBO MX2001 monitoring.

## Data path

```text
HOBO MX2001
    -> field Meshtastic node
    -> LoRa mesh
    -> Heltec Hub (heltec-v4-tft)
    -> HTTPS over home Wi-Fi
    -> https://meshtastic-ecru.vercel.app/api/ingest
    -> Neon PostgreSQL / dashboard
```

No Raspberry Pi or always-on PC is required after the Heltec is flashed and configured.

## What is uploaded

This finalized build is MX2001-only.

- The gateway listens promiscuously for decoded `PRIVATE_APP` packets.
- Only packets exactly 19 bytes long and beginning with ASCII `MX` are accepted as MX2001 records.
- Normal Meshtastic `TELEMETRY_APP` packets are ignored, including environmental temperature telemetry from neighboring nodes.
- NodeDB Favorites are not required. The custom MX2001 packet format itself identifies eligible data.
- MX2001 records include stage, temperature, raw temperature value, logger MAC, measurement sequence and BLE RSSI.
- Radio metadata is included: RSSI, SNR, hop start/limit, hops away, relay node, channel, gateway name and packet ID.
- A small in-memory packet-ID ring prevents normal LoRa duplicate/rebroadcast copies from being uploaded twice.
- HTTPS uploads run on a separate OSThread so the mesh packet handler does not wait on the internet request.
- Failed uploads are retried up to four times. The HOBO remains the authoritative logger during longer internet outages.

## Secret configuration

Never commit the Vercel ingest key.

The module looks for the local secret header at:

```text
src/modules/hobo_gateway_secrets.h
```

That path is ignored by git.

Set `HOBO_HTTP_GATEWAY_INGEST_KEY` to the same value as the Vercel project's `INGEST_KEY` environment variable.

A minimal local secret header is:

```cpp
#pragma once
#define HOBO_HTTP_GATEWAY_ENABLED 1
#define HOBO_HTTP_GATEWAY_INGEST_KEY "YOUR_LOCAL_SECRET"
#define HOBO_HTTP_GATEWAY_NAME "Heltec Hub (Home)"
```

Do not commit the real secret header.

## Build

On the Windows build used for this gateway:

```powershell
$env:PLATFORMIO_CORE_DIR="C:\p"
$env:PLATFORMIO_BUILD_UNFLAGS="-std=c++11 -std=gnu++11 -flto"
py -m platformio run -e heltec-v4-tft -j 1
```

## Flash update image

Put the Heltec in the ESP32-S3 ROM bootloader:

1. Hold LEFT/PRG.
2. Tap RIGHT/RST once.
3. Release LEFT/PRG.
4. Identify the COM port.

Then flash the application image at `0x10000` using the generated `firmware-heltec-v4-tft-*.bin` file.

Example:

```powershell
py -m esptool --port COM20 write-flash 0x10000 .\.pio\build\heltec-v4-tft\firmware-heltec-v4-tft-<version>.bin
```

Use the actual COM port if it is not COM20.

## Verification

Watch the Heltec serial log after a fresh automatic MX2001 field measurement. Expected messages include:

```text
HOBO HTTP gateway: queued MX2001 packet from ...
HOBO HTTP gateway: cloud stored packet ... (HTTP 201)
```

Normal environmental telemetry should not produce a gateway queue/upload message.

## TLS note

The current build uses `WiFiClientSecure::setInsecure()`: HTTPS traffic is encrypted, but the server certificate chain is not validated by the Heltec. API writes are still protected by the `X-Ingest-Key` application secret. A pinned CA/certificate-validation upgrade can be added later for hardened production deployment.
