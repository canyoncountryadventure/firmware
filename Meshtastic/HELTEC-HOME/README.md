# Heltec Home MX2001 Gateway

**Status:** Production / end-to-end validated 2026-08-22  
**Branch:** `heltec-home-http-gateway`  
**Hardware:** Heltec WiFi LoRa 32 V4 + TFT  
**PlatformIO target:** `heltec-v4-tft`

This is the home internet gateway for MX2001 monitoring traffic.

## Role

```text
Field MX2001 node -> LoRa mesh -> Heltec Home -> Wi-Fi HTTPS -> Vercel -> Neon
```

The Heltec remains a normal Meshtastic radio while also watching decoded mesh traffic for the custom MX2001 packet format.

## Accepted packet format

Only `PRIVATE_APP` packets that are exactly 19 bytes and begin with ASCII `MX` are uploaded.

The current packet contains:

| Field | Purpose |
|---|---|
| `MX` prefix | identifies the custom MX2001 format |
| sequence | field-node measurement sequence |
| stage | water level in tenths of a foot |
| temperature | temperature in tenths of a degree F |
| raw temperature | logger raw value |
| logger MAC | physical HOBO BLE identifier |
| BLE RSSI | field radio to HOBO signal strength |

Meshtastic receive metadata is added by the Heltec before upload.

## Deliberately ignored

The gateway does not upload:

- `TELEMETRY_APP` environmental temperature from other Meshtastic nodes;
- device/battery telemetry;
- positions;
- NodeInfo;
- text messages;
- routing/ACK traffic;
- arbitrary `PRIVATE_APP` packets that do not match the MX2001 format.

Favorites are not used as an authorization mechanism. A future MX2001 field node running the same packet format can feed the gateway without first being added to the Heltec NodeDB.

## Local configuration

Create this file locally and never commit it:

```text
src/modules/hobo_gateway_secrets.h
```

It supplies the Vercel ingest key and gateway name. The path is listed in `.gitignore`.

## Sync

```powershell
cd C:\mt
git fetch origin
git switch heltec-home-http-gateway
git pull --ff-only origin heltec-home-http-gateway
```

## Build

```powershell
$env:PLATFORMIO_CORE_DIR="C:\p"
$env:PLATFORMIO_BUILD_UNFLAGS="-std=c++11 -std=gnu++11 -flto"
py -m platformio run -e heltec-v4-tft -j 1
```

The non-factory update image is generated under:

```text
.pio\build\heltec-v4-tft\firmware-heltec-v4-tft-*.bin
```

## Flash

Enter bootloader mode:

1. Hold LEFT/PRG.
2. Tap RIGHT/RST.
3. Release LEFT/PRG.

Flash at `0x10000`:

```powershell
py -m esptool --port COM20 write-flash 0x10000 .\.pio\build\heltec-v4-tft\firmware-heltec-v4-tft-<version>.bin
```

Use the actual COM port if different. Use the normal `.bin`, not `.factory.bin`, for an application update that preserves existing Meshtastic configuration.

## Live verification

```powershell
py -m meshtastic --port COM20 --seriallog stdout --listen 2>&1 |
Select-String -Pattern "HOBO HTTP|PRIVATE_APP"
```

Expected sequence:

```text
HOBO HTTP gateway: queued MX2001 packet from ...
HOBO HTTP gateway: cloud stored packet ... (HTTP 201)
```

## Cloud behavior

The Heltec POSTs the decoded MX2001 data and radio metadata to the Vercel ingest endpoint. The API writes the reading to Neon PostgreSQL.

The HOBO remains the authoritative logger. If home internet is unavailable, the gateway retries a small number of times, but the original measurements remain stored on the HOBO itself.

## Security note

The current firmware uses encrypted HTTPS with certificate verification disabled via `setInsecure()`. The ingest API still requires the application-layer secret key. Certificate validation can be hardened later without changing the packet architecture.

## Future sensor expansion

Do not broaden the MX2001 filter just to support a new sensor. Add each future sensor family with an explicit packet signature/type so unrelated public Meshtastic traffic remains excluded from the monitoring database.