# Heltec Home MX2001 Gateway

**Status:** Production / end-to-end validated 2026-08-22; recovery notes updated 2026-08-26  
**Branch:** `heltec-home-http-gateway`  
**Hardware:** Heltec WiFi LoRa 32 V4 OLED  
**PlatformIO target:** `heltec-v4`

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

- `TELEMETRY_APP` environmental temperature from unrelated nodes;
- device/battery telemetry;
- positions;
- NodeInfo;
- text messages;
- routing/ACK traffic;
- arbitrary `PRIVATE_APP` packets that do not match the MX2001 format.

## Important OLED target correction

The physical home gateway is the **OLED Heltec V4**, therefore use:

```text
heltec-v4
```

not:

```text
heltec-v4-tft
```

On 2026-08-26 the original module was found to be enabled only by `HELTEC_V4_TFT`, which allowed the OLED build to succeed while silently omitting the HTTP gateway. The branch was updated to use the common `HELTEC_V4` define so the module is available on the OLED target.

## Local configuration

Create this file locally and never commit it:

```text
src/modules/hobo_gateway_secrets.h
```

Minimal content:

```cpp
#pragma once
#define HOBO_HTTP_GATEWAY_INGEST_KEY "YOUR_LOCAL_SECRET"
```

The value must match Vercel production `INGEST_KEY`.

If the local file is missing or blank, the updated firmware emits:

```text
HOBO HTTP gateway: INGEST_KEY is empty; cloud uploads are disabled
```

## Sync

```powershell
cd C:\Meshtastic-HOBO\firmware
git fetch cca heltec-home-http-gateway
git switch heltec-home-http-gateway
git pull --ff-only cca heltec-home-http-gateway
```

## Build

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e heltec-v4
```

## Flash by USB

For the known Heltec on `COM20`:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run `
  -e heltec-v4 `
  -t upload `
  --upload-port COM20
```

Use the actual COM port if different. Do not erase the full flash unless intentionally wiping Meshtastic configuration.

## Live verification

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor -p COM20 -b 115200
```

Expected startup:

```text
HOBO HTTP gateway enabled: MX2001-only -> https://meshtastic-ecru.vercel.app/api/ingest
```

Expected packet sequence:

```text
HOBO HTTP gateway: queued MX2001 packet from ...
HOBO HTTP gateway: cloud stored packet ... (HTTP 201)
```

## Wi-Fi OTA quick diagnostic

Normal Meshtastic TCP API:

```text
port 4403
```

Unified OTA loader:

```text
port 3232
```

Check both:

```powershell
Test-NetConnection 192.168.1.147 -Port 4403
Test-NetConnection 192.168.1.147 -Port 3232
```

If an OTA attempt fails with `WinError 10061`, then `4403=True` and `3232=False`, the radio has returned to normal Meshtastic but the OTA loader is not available. Use USB recovery rather than repeatedly retrying Wi-Fi OTA.

The verified 16 MB partition layout places:

```text
app0 at 0x10000
app1 at 0x650000
```

The 2026-08-26 repair installed Meshtastic's `mt-esp32s3-ota.bin` into `app1` at `0x650000` after verifying that offset in the local manifest. Do not assume that offset for another partition layout without checking it first, and do not run `erase_flash` for this repair.

## When data stops appearing in Neon

Use this order:

1. Confirm the field node is transmitting its `MX` packet.
2. Confirm the Heltec serial log contains `HOBO HTTP gateway enabled`.
3. Confirm the local ingest key exists.
4. Check Vercel logs for POSTs to `/api/ingest`.
5. No POSTs = Heltec/module/key/Wi-Fi problem.
6. `HTTP 401` = ingest key mismatch.
7. `HTTP 201` = Vercel accepted the reading; then inspect Neon/dashboard.

This branch stays MX2001-only. Future sensor types should use separate explicit packet signatures rather than broadening the MX2001 filter.

## Security note

The current firmware uses encrypted HTTPS with certificate verification disabled via `setInsecure()`. The ingest API still requires the application-layer secret key.
