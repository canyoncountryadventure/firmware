# Heltec Home MX2001 Gateway

**Status:** Production / end-to-end validated 2026-08-22; diagnostics updated 2026-08-26  
**Branch:** `heltec-home-http-gateway`  
**Hardware:** Heltec WiFi LoRa 32 V4 OLED  
**PlatformIO target:** `heltec-v4`

This branch is the original CCA Meshtastic-to-cloud gateway for HOBO MX2001 water-level monitoring. It remains deliberately MX2001-only: it forwards the custom MX packet containing water level, temperature and HOBO/logger metadata while ignoring unrelated public Meshtastic telemetry.

## Data path

```text
HOBO MX2001
   -> BLE
Field Meshtastic node
   -> LoRa mesh
Heltec Home Gateway
   -> Wi-Fi / HTTPS
Vercel ingest API
   -> Neon PostgreSQL
```

No Raspberry Pi or always-on PC is required after deployment.

## What this gateway uploads

Only decoded Meshtastic packets that are:

- `PRIVATE_APP`;
- exactly 19 bytes;
- prefixed with ASCII `MX`.

The packet carries:

- water level / stage;
- temperature;
- raw temperature value;
- logger BLE MAC;
- measurement sequence;
- BLE RSSI;
- Meshtastic radio metadata including RSSI, SNR, hop start/limit, hops away, relay node, channel and packet ID.

Normal `TELEMETRY_APP`, position, NodeInfo, text, routing and device telemetry are intentionally ignored.

## Critical Heltec V4 target note

The deployed home gateway is the **OLED Heltec V4**, so build with:

```text
-e heltec-v4
```

Do not use `heltec-v4-tft` for this unit.

During diagnostics on 2026-08-26, the custom HTTP gateway was found to have been gated only by `HELTEC_V4_TFT`. That can produce a successful OLED firmware build while silently compiling the gateway module out, resulting in no POST attempts to Vercel. The applicable fix is to enable the module from the common `HELTEC_V4` define so both V4 variants can compile it while the physical OLED gateway uses `heltec-v4`.

## Local secret is mandatory

The Vercel ingest key belongs only in the git-ignored local file:

```text
src/modules/hobo_gateway_secrets.h
```

Minimal form:

```cpp
#pragma once
#define HOBO_HTTP_GATEWAY_INGEST_KEY "YOUR_LOCAL_SECRET"
```

Never commit the real key. If this file is missing or `HOBO_HTTP_GATEWAY_INGEST_KEY` is empty, the gateway cannot upload to Vercel.

A useful local check that does not print the key:

```powershell
$secret = ".\src\modules\hobo_gateway_secrets.h"
if (Test-Path $secret) {
    $content = Get-Content $secret -Raw
    if ($content -match 'HOBO_HTTP_GATEWAY_INGEST_KEY\s+"([^"]+)"') {
        Write-Host "INGEST KEY PRESENT - length:" $matches[1].Length
    } else {
        Write-Host "SECRET FILE EXISTS BUT KEY WAS NOT FOUND"
    }
} else {
    Write-Host "SECRET FILE MISSING"
}
```

If the local file is lost, restore the key from the Vercel project's production `INGEST_KEY`; do not put it in GitHub.

## Build on Windows

```powershell
cd C:\Meshtastic-HOBO\firmware

& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e heltec-v4
```

## USB flash

For the known Heltec on `COM20`:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run `
  -e heltec-v4 `
  -t upload `
  --upload-port COM20
```

Use the actual COM port if it changes. Do not erase the full flash unless intentionally wiping Meshtastic configuration.

## Serial verification

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor -p COM20 -b 115200
```

Expected startup when the gateway module is present:

```text
HOBO HTTP gateway enabled: MX2001-only -> https://meshtastic-ecru.vercel.app/api/ingest
```

Expected packet flow:

```text
HOBO HTTP gateway: queued MX2001 packet from ...
HOBO HTTP gateway: cloud stored packet ... (HTTP 201)
```

## Wi-Fi OTA and recovery diagnostics

Normal Meshtastic TCP API uses port `4403`. The ESP32 Unified OTA loader uses TCP port `3232` while the device is in OTA mode.

```powershell
Test-NetConnection 192.168.1.147 -Port 4403
Test-NetConnection 192.168.1.147 -Port 3232
```

During the 2026-08-26 recovery, Wi-Fi OTA failed with:

```text
OTA update failed: [WinError 10061] No connection could be made because the target machine actively refused it
```

The device then showed:

```text
4403 = True
3232 = False
```

That means normal Meshtastic is back but the Unified OTA loader did not remain available. Do not repeatedly retry Wi-Fi OTA in that state.

For the 16 MB Heltec V4 layout used here, the build manifest confirmed:

```text
app0  ota_0  0x10000  0x640000
app1  ota_1  0x650000 0x640000
```

Only after confirming the local partition manifest, the Unified OTA ESP32-S3 loader can be installed once by USB to `app1` at `0x650000`. The 2026-08-26 repair wrote `mt-esp32s3-ota.bin` there with esptool and verified the hash. Do not use `erase_flash` for this repair.

After the OTA loader exists, later `--ota-update` operations can use Wi-Fi. USB flashing remains the simplest recovery path.

## Cloud diagnostic sequence

If the Seeed/field node is transmitting but Neon remains empty:

1. Check Vercel runtime logs for POSTs to `/api/ingest`.
2. **Zero POSTs** means the problem is on the Heltec before Vercel: gateway module compiled out/not started, missing ingest key, or no Wi-Fi connection.
3. `HTTP 401` means the Heltec key does not match Vercel `INGEST_KEY`.
4. `HTTP 201` means Vercel accepted and stored the reading; then verify Neon/dashboard display.
5. Serial startup should always be used to verify that the HTTP gateway module actually exists in the running build.

## Validated result

The original path has been confirmed end-to-end:

```text
MX2001 -> field node -> LoRa -> Heltec -> HTTPS -> Vercel -> Neon
```

A live automatic MX2001 record was received by the Heltec, queued by the HTTP gateway, returned `HTTP 201`, and appeared in Neon with stage, temperature, logger metadata and radio metadata intact.

## Related documentation

- [`Meshtastic/HELTEC-HOME/README.md`](Meshtastic/HELTEC-HOME/README.md)
- [`docs/heltec-home-http-gateway.md`](docs/heltec-home-http-gateway.md)
- Field-node firmware branch: `hobo-mx2001-mx2201-mx2203`

## Security note

The current firmware uses `WiFiClientSecure::setInsecure()`. HTTPS transport is encrypted, but the server certificate chain is not validated by the Heltec. API writes are still authenticated by `X-Ingest-Key`. Certificate validation can be hardened later without changing the MX2001 packet architecture.
