# CCA Heltec V4 Meshtastic-to-Cloud Gateway

**Branch:** `heltec-home-http-gateway-rock`  
**Hardware:** Heltec WiFi LoRa 32 V4 OLED  
**PlatformIO target:** `heltec-v4`  
**Cloud path:** Heltec -> Vercel ingest -> Neon PostgreSQL -> Vercel dashboard

This branch extends the proven CCA Heltec home HTTP gateway so it can forward sandstone-moisture/PIR data, HOBO MX2001 custom records, and standard environmental temperature telemetry from Meshtastic field nodes.

## Data path

```text
Seeed / field node sensors
    -> Meshtastic LoRa
    -> Heltec V4 OLED
    -> Wi-Fi / HTTPS
    -> Vercel /api/ingest
    -> Neon PostgreSQL
    -> Vercel dashboard
```

No Raspberry Pi or always-on PC is required after deployment.

## Supported telemetry

The gateway currently accepts:

- `PRIVATE_APP` 16-byte `RK` packets for Navajo sandstone moisture + PIR;
- `PRIVATE_APP` 19-byte `MX` packets for HOBO MX2001 records;
- `TELEMETRY_APP` environmental metrics for temperature, including the MX2201 temperature path.

The sandstone dashboard uses `type: "rock_test"` and stores rock ADC, sensor voltage, motion state/count, battery fields, and LoRa metadata.

## Critical target note

The physical gateway used here is the **OLED Heltec V4**, so build:

```text
-e heltec-v4
```

Do not use `heltec-v4-tft` for this unit.

On 2026-08-26 a diagnostic session found that the custom gateway had accidentally been enabled only for `HELTEC_V4_TFT`, which meant a normal `heltec-v4` flash succeeded while silently compiling the HTTP gateway out. This was fixed in commit:

```text
054cf18c8ec3422946e0586a83326a915475609b
Enable HTTP gateway on Heltec V4 OLED
```

## Local secret

The production Vercel ingest key belongs only in the git-ignored local file:

```text
src/modules/hobo_gateway_secrets.h
```

Minimal form:

```cpp
#pragma once
#define HOBO_HTTP_GATEWAY_INGEST_KEY "YOUR_LOCAL_SECRET"
```

Never commit the real key. If this file is missing or the key is empty, the gateway will not attempt HTTP uploads.

## Build on Windows

```powershell
cd C:\Meshtastic-HOBO\firmware

& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e heltec-v4
```

## USB flash

Example using the Heltec on `COM20`:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run `
  -e heltec-v4 `
  -t upload `
  --upload-port COM20
```

Do not erase the full flash unless there is a specific reason to wipe stored configuration.

## Serial verification

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor -p COM20 -b 115200
```

Expected startup:

```text
CCA HTTP gateway enabled: MX2001 + ROCK + environment -> https://meshtastic-ecru.vercel.app/api/ingest
```

Expected successful upload:

```text
CCA HTTP gateway: queued ROCK ADC=...
CCA HTTP gateway: cloud stored packet ... (HTTP 201)
```

## Wi-Fi OTA / recovery quick reference

Normal Meshtastic TCP API uses port `4403`. The ESP32 Unified OTA loader uses port `3232`.

```powershell
Test-NetConnection 192.168.1.147 -Port 4403
Test-NetConnection 192.168.1.147 -Port 3232
```

During the 2026-08-26 recovery, Wi-Fi OTA failed with `WinError 10061`; afterward `4403=True` and `3232=False`. The 16 MB partition table confirmed:

```text
app0  ota_0  0x10000
app1  ota_1  0x650000
```

The ESP32-S3 Unified OTA loader was then written only to `app1` at `0x650000`, preserving the normal application and configuration. Never assume that offset for another target; verify its partition table first.

## Detailed diagnostics

The complete diagnostic/recovery record is here:

- [`docs/heltec-home-http-gateway.md`](docs/heltec-home-http-gateway.md)

It includes:

- the OLED-vs-TFT compile-gate failure;
- the missing local ingest-key failure;
- Vercel/Neon decision logic;
- `4403`/`3232` OTA diagnostics;
- the confirmed Heltec V4 partition map;
- one-time Unified OTA loader repair at `0x650000`;
- safe Vercel secret restoration without printing the key;
- build, USB flash, serial-monitor, and expected `HTTP 201` verification commands.

## Security note

The current HTTP gateway uses `WiFiClientSecure::setInsecure()`: HTTPS is encrypted, but the Heltec does not validate the server certificate chain. API writes are still protected by the Vercel `X-Ingest-Key`. Certificate validation can be hardened later.
