# Heltec Home HTTP Gateway — MX2001

**Branch:** `heltec-home-http-gateway`  
**Hardware:** Heltec WiFi LoRa 32 V4 OLED  
**PlatformIO target:** `heltec-v4`

This is the original CCA Meshtastic-to-cloud gateway for HOBO MX2001 water-level monitoring.

## Data path

```text
HOBO MX2001
    -> BLE
Field Meshtastic node
    -> LoRa mesh
Heltec V4 OLED
    -> HTTPS over Wi-Fi
Vercel /api/ingest
    -> Neon PostgreSQL
```

## What is uploaded

This branch remains deliberately MX2001-only.

- Gateway listens promiscuously for decoded `PRIVATE_APP` traffic.
- Only packets exactly 19 bytes long beginning with ASCII `MX` are accepted.
- Packet fields include stage/water level, temperature, raw temperature, logger MAC, sequence and BLE RSSI.
- Heltec adds LoRa RSSI, SNR, hop start/limit, hops away, relay node, channel, packet ID and gateway name.
- Duplicate/rebroadcast packet IDs are suppressed in memory.
- Uploads use a separate OSThread and retry up to four times.
- Normal `TELEMETRY_APP`, position, NodeInfo, text, routing and device telemetry are ignored.

## 2026-08-26 Heltec V4 target correction

Diagnostics on the physical home gateway established that it is the OLED Heltec V4 and should be built with:

```text
-e heltec-v4
```

The earlier code gated the custom module on `HELTEC_V4_TFT`. That can let an OLED firmware build succeed while silently compiling the HTTP gateway out. The module should use the common `HELTEC_V4` define; the OLED target itself defines `HELTEC_V4` plus `HELTEC_V4_OLED`.

A key symptom of the compile-gate problem is: field node packets are transmitting, but Vercel shows **zero POST requests** to `/api/ingest`.

## Secret configuration

The production ingest key must remain local and git-ignored:

```text
src/modules/hobo_gateway_secrets.h
```

Minimal file:

```cpp
#pragma once
#define HOBO_HTTP_GATEWAY_INGEST_KEY "YOUR_LOCAL_SECRET"
```

The value must equal the Vercel project's production `INGEST_KEY`.

If the file is missing or the key is blank, uploads are disabled. The updated gateway firmware logs this at startup:

```text
HOBO HTTP gateway: INGEST_KEY is empty; cloud uploads are disabled
```

Never commit the real key.

## Windows build

```powershell
cd C:\Meshtastic-HOBO\firmware

& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e heltec-v4
```

## USB flash

Example for the known unit on `COM20`:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run `
  -e heltec-v4 `
  -t upload `
  --upload-port COM20
```

Do not run `erase_flash` unless intentionally wiping the stored Meshtastic configuration.

## Serial verification

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor -p COM20 -b 115200
```

Expected startup:

```text
HOBO HTTP gateway enabled: MX2001-only -> https://meshtastic-ecru.vercel.app/api/ingest
```

Expected successful packet flow:

```text
HOBO HTTP gateway: queued MX2001 packet from ...
HOBO HTTP gateway: cloud stored packet ... (HTTP 201)
```

## Wi-Fi OTA diagnostics

Normal Meshtastic TCP API uses port `4403`. Unified OTA uses TCP port `3232` while the ESP32-S3 OTA loader is active.

```powershell
Test-NetConnection 192.168.1.147 -Port 4403
Test-NetConnection 192.168.1.147 -Port 3232
```

A failed OTA session on 2026-08-26 produced:

```text
OTA update failed: [WinError 10061] No connection could be made because the target machine actively refused it
```

and then:

```text
4403 = True
3232 = False
```

Interpretation: the radio returned to normal Meshtastic, but the Unified OTA loader was not available. Repeated Wi-Fi OTA attempts are not useful until the loader is repaired.

## Unified OTA loader recovery

The local 16 MB partition manifest was verified as:

```text
nvs      0x9000
otadata  0xe000
app0     0x10000  size 0x640000
app1     0x650000 size 0x640000
spiffs   0xc90000
coredump 0xFF0000
```

For this exact verified layout, `app1` starts at `0x650000`.

The successful one-time repair was:

1. Download Meshtastic's ESP32-S3 Unified OTA loader `mt-esp32s3-ota.bin`.
2. Verify the local build manifest still reports `app1 = 0x650000`.
3. Connect by USB.
4. Write only the OTA loader to `0x650000` with esptool.
5. Do **not** erase the whole flash.

Example after resolving the local esptool path:

```powershell
py "$($esptool.FullName)" `
  --chip esp32s3 `
  --port COM20 `
  --baud 921600 `
  write_flash `
  0x650000 `
  ".\mt-esp32s3-ota.bin"
```

The successful repair reported the data hash verified and hard-reset the device. Once the loader is installed, later Wi-Fi OTA is possible; USB remains the recovery path.

## Cloud-side troubleshooting order

When the field node is known to be transmitting:

1. Inspect Heltec serial startup. If the `HOBO HTTP gateway enabled` line is absent, the custom module was not compiled/started.
2. Check that the local secret file exists and key is non-empty.
3. Inspect Vercel runtime logs for `/api/ingest`.
4. **No POSTs:** failure is before Vercel — module, key or Heltec Wi-Fi.
5. `401 Unauthorized`: ingest key mismatch.
6. `201`: Vercel accepted/stored the packet; next verify Neon/dashboard.

This ordering avoids wasting time debugging Neon when the Heltec never attempted an HTTP request.

## TLS note

The current build uses `WiFiClientSecure::setInsecure()`. Traffic is encrypted, but the server certificate chain is not validated by the Heltec. API writes are authenticated using the `X-Ingest-Key` secret.
