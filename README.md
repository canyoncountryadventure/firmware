# Heltec Home MX2001 Gateway

**Status:** Production / end-to-end validated 2026-08-22  
**Branch:** `heltec-home-http-gateway`  
**Hardware:** Heltec WiFi LoRa 32 V4 + TFT  
**PlatformIO target:** `heltec-v4-tft`

This branch turns a Heltec V4 into a direct Meshtastic-to-cloud gateway for HOBO MX2001 water-level monitoring.

## Data path

```text
HOBO MX2001
   -> BLE
Field Meshtastic node
   -> LoRa mesh
Heltec Home Gateway
   -> Wi-Fi / HTTPS
Vercel ingest API
   ->
Neon PostgreSQL
```

No Raspberry Pi or always-on PC is required after deployment.

## What this gateway uploads

The finalized gateway is deliberately **MX2001-only**.

It accepts only decoded Meshtastic packets that are:

- `PRIVATE_APP`;
- exactly 19 bytes;
- prefixed with ASCII `MX`.

Those packets carry:

- water level / stage;
- temperature;
- raw temperature value;
- logger BLE MAC;
- measurement sequence;
- BLE RSSI;
- Meshtastic radio metadata including RSSI, SNR, hop start/limit, hops away, relay node, channel and packet ID.

The gateway ignores normal Meshtastic environmental telemetry, position, NodeInfo, text messages, routing traffic and device telemetry. Node Favorites are not required; the custom MX2001 packet format identifies eligible data.

## Validated result

The direct path has been confirmed end-to-end:

```text
MX2001 -> field node -> LoRa -> Heltec -> HTTPS -> Vercel -> Neon
```

A live automatic MX2001 record was received by the Heltec, queued by the HTTP gateway, returned `HTTP 201`, and appeared in Neon with stage, temperature, logger metadata and radio metadata intact.

## Start here

- Heltec deployment guide: [`Meshtastic/HELTEC-HOME/README.md`](Meshtastic/HELTEC-HOME/README.md)
- Detailed HTTP gateway notes: [`docs/heltec-home-http-gateway.md`](docs/heltec-home-http-gateway.md)
- Field-node firmware lives on branch: `hobo-mx2001-mx2201-mx2203`

## Local secret

The Vercel ingest key belongs only in:

```text
src/modules/hobo_gateway_secrets.h
```

That file is git-ignored. Never commit the real key.

## Build on Windows

The validated Windows build uses a short PlatformIO core path and disables LTO because the ESP32-S3 Windows linker otherwise failed to launch its LTO helper.

```powershell
cd C:\mt
$env:PLATFORMIO_CORE_DIR="C:\p"
$env:PLATFORMIO_BUILD_UNFLAGS="-std=c++11 -std=gnu++11 -flto"
py -m platformio run -e heltec-v4-tft -j 1
```

## Flash

Put the Heltec into the ESP32-S3 ROM bootloader:

1. Hold LEFT/PRG.
2. Tap RIGHT/RST.
3. Release LEFT/PRG.

Then flash the non-factory application image at `0x10000`:

```powershell
py -m esptool --port COM20 write-flash 0x10000 .\.pio\build\heltec-v4-tft\firmware-heltec-v4-tft-<version>.bin
```

Use the actual COM port if different.

## Verification

Expected serial lines after an automatic MX2001 record:

```text
HOBO HTTP gateway: queued MX2001 packet from ...
HOBO HTTP gateway: cloud stored packet ... (HTTP 201)
```

Normal environmental telemetry should not generate a gateway upload.

## Future sensors

This branch is intentionally frozen around the proven MX2001 path. Future wired sensors such as soil moisture or trail counters can be added later using their own explicit custom packet signatures and ingest types without weakening the current MX2001 filter.

The rest of the repository remains the full Meshtastic source tree because the normal firmware source, variants, libraries and PlatformIO configuration are required to compile the gateway.