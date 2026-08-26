# CCA Heltec V4 HTTP Gateway

This document is the operational and recovery guide for the CCA Meshtastic home gateway running on a **Heltec WiFi LoRa 32 V4 OLED**.

## Current architecture

```text
Seeed XIAO nRF52840 + Wio-SX1262
    |- Grove capacitive sandstone probe -> PRIVATE_APP `RK`
    |- SEN0171 PIR -> included in `RK`
    |- HOBO MX2201 temperature -> TELEMETRY_APP
    `- HOBO MX2001 custom record -> PRIVATE_APP `MX`
             |
             v
        Meshtastic LoRa
             |
             v
Heltec WiFi LoRa 32 V4 OLED
PlatformIO target: heltec-v4
             |
             v
Wi-Fi / HTTPS POST
https://meshtastic-ecru.vercel.app/api/ingest
             |
             v
Vercel ingest API -> Neon PostgreSQL -> Vercel dashboard
```

No Raspberry Pi or always-on PC is required once the gateway is configured.

## Branch and important fix

Current gateway branch:

```text
heltec-home-http-gateway-rock
```

The OLED Heltec must be built with:

```text
-e heltec-v4
```

Do **not** use `heltec-v4-tft` for the OLED unit.

A diagnostic session on 2026-08-26 found that the HTTP gateway module had accidentally been compiled only when `HELTEC_V4_TFT` was defined. The actual `heltec-v4` target defines `HELTEC_V4` and `HELTEC_V4_OLED`, so the firmware flashed and ran normally but the HTTP gateway did not exist in that build.

Fixed in commit:

```text
054cf18c8ec3422946e0586a83326a915475609b
Enable HTTP gateway on Heltec V4 OLED
```

The gateway enable guard must use the common Heltec V4 definition, not the TFT-only definition.

## Supported mesh packets

The gateway is promiscuous and currently accepts these decoded packet types from other mesh nodes:

### `RK` sandstone packet

- Meshtastic port: `PRIVATE_APP`
- length: 16 bytes
- bytes 0-1: ASCII `RK`
- byte 2: schema version `1`

Payload:

```text
0..1   'R','K'
2      schema 1
3      flags bit0 = current PIR motion
4..5   rock ADC, LE16
6..7   sensor millivolts, LE16
8..11  motion count since boot, LE32
12..13 battery millivolts, LE16
14     battery percent
15     reserved
```

The HTTP upload uses:

```json
{
  "type": "rock_test",
  "payload": {
    "rock_adc": 0,
    "rock_voltage_v": 0.0,
    "motion_detected": false,
    "motion_count": 0,
    "battery_voltage_v": 0.0,
    "battery_percent": 0
  }
}
```

### `MX` custom HOBO packet

- Meshtastic port: `PRIVATE_APP`
- length: 19 bytes
- bytes 0-1: ASCII `MX`

Uploads stage, temperature, raw temperature, logger BLE MAC, sequence, BLE RSSI, and LoRa metadata as `type: "mx2001"`.

### Standard environmental telemetry

- Meshtastic port: `TELEMETRY_APP`
- protobuf `environment_metrics`

The gateway extracts `environment_metrics.temperature` and uploads it as `type: "telemetry"`.

This is used for the nearby HOBO MX2201 temperature path when the field node emits standard Meshtastic environmental telemetry.

## Local ingest secret

The Vercel ingest key must never be committed.

Local file:

```text
src/modules/hobo_gateway_secrets.h
```

Minimal contents:

```cpp
#pragma once
#define HOBO_HTTP_GATEWAY_INGEST_KEY "YOUR_LOCAL_SECRET"
```

The module intentionally refuses to upload when `HOBO_HTTP_GATEWAY_INGEST_KEY` is empty. An absent local secret can therefore look like a healthy Meshtastic gateway that simply never makes HTTP requests.

### Safe presence check

This checks the header without printing the key:

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

### Restore the key from Vercel without displaying it

Node.js/npm/npx are required for this method. If `npx` is missing:

```powershell
winget install -e --id OpenJS.NodeJS.LTS
```

Close and reopen PowerShell after installation.

Link a temporary directory to the production Vercel project:

```powershell
$tmpDir = "$env:TEMP\cca-meshtastic-vercel"
New-Item -ItemType Directory -Force -Path $tmpDir | Out-Null

npx vercel link `
  --yes `
  --project meshtastic `
  --scope canyon-country-adventures-projects `
  --cwd $tmpDir

npx vercel env pull "$tmpDir\production.env" `
  --environment=production `
  --yes `
  --cwd $tmpDir
```

Extract only `INGEST_KEY`, create the local header, and do not print the key:

```powershell
$envLine = Get-Content "$tmpDir\production.env" |
    Where-Object { $_ -match '^INGEST_KEY=' } |
    Select-Object -First 1

if (-not $envLine) {
    throw "INGEST_KEY was not found in Vercel production environment."
}

$key = ($envLine -replace '^INGEST_KEY=', '').Trim()

if (
    ($key.StartsWith('"') -and $key.EndsWith('"')) -or
    ($key.StartsWith("'") -and $key.EndsWith("'"))
) {
    $key = $key.Substring(1, $key.Length - 2)
}

if ([string]::IsNullOrWhiteSpace($key)) {
    throw "INGEST_KEY is empty."
}

$header = @"
#pragma once
#define HOBO_HTTP_GATEWAY_INGEST_KEY "$key"
"@

Set-Content `
  "C:\Meshtastic-HOBO\firmware\src\modules\hobo_gateway_secrets.h" `
  $header `
  -Encoding ASCII

Write-Host "INGEST KEY RESTORED - length:" $key.Length

Remove-Item $tmpDir -Recurse -Force
Remove-Variable key
```

Never commit `hobo_gateway_secrets.h`.

## Windows build

Repository used during validation:

```text
C:\Meshtastic-HOBO\firmware
```

Build the OLED V4 target:

```powershell
cd C:\Meshtastic-HOBO\firmware

& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e heltec-v4
```

## USB application flash

Example port used during the 2026-08-26 recovery was `COM20`. Use the actual Heltec COM port if different.

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run `
  -e heltec-v4 `
  -t upload `
  --upload-port COM20
```

Do not erase the whole flash unless there is a specific reason to destroy stored configuration.

## Serial verification

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor -p COM20 -b 115200
```

Expected startup line:

```text
CCA HTTP gateway enabled: MX2001 + ROCK + environment -> https://meshtastic-ecru.vercel.app/api/ingest
```

Expected rock traffic:

```text
CCA HTTP gateway: queued ROCK ADC=... from ...
CCA HTTP gateway: cloud stored packet ... (HTTP 201)
```

Expected environmental temperature traffic:

```text
CCA HTTP gateway: queued environment temp=... C from ...
CCA HTTP gateway: cloud stored packet ... (HTTP 201)
```

If packets are being queued but uploads fail, inspect the HTTP status/error in the serial log.

## Wi-Fi OTA architecture

Normal Meshtastic TCP API:

```text
TCP 4403
```

ESP32 Unified OTA loader:

```text
TCP 3232
```

A normal Wi-Fi OTA sequence is:

```text
normal firmware on :4403
    -> AdminModule requests OTA mode
    -> reboot into app1 Unified OTA loader
    -> loader listens on :3232
    -> Python CLI uploads firmware
    -> reboot into updated normal firmware
```

### Diagnostic commands

```powershell
Test-NetConnection 192.168.1.147 -Port 4403
Test-NetConnection 192.168.1.147 -Port 3232
```

Interpretation:

| 4403 | 3232 | Meaning |
|---|---|---|
| True | False | Normal Meshtastic firmware is running; OTA loader is not currently listening. If this occurs after an OTA trigger/failure, suspect missing/invalid OTA loader or failure to switch to it. |
| False | True | Device is in Unified OTA loader mode. Retry the OTA upload promptly. |
| False | False | Device may be rebooting, offline, on another IP, or otherwise unreachable. |

## 2026-08-26 OTA failure and recovery

### Symptom

The command:

```powershell
py -m meshtastic --host 192.168.1.147 --ota-update "<firmware.bin>"
```

connected to the radio, triggered OTA, waited for reboot, then repeatedly printed `Starting OTA update...` and ended with:

```text
OTA update failed: [WinError 10061] No connection could be made because the target machine actively refused it
```

Port testing immediately afterward showed:

```text
4403 -> TcpTestSucceeded : True
3232 -> TcpTestSucceeded : False
```

This proved the Heltec had returned to normal Meshtastic mode and the actual firmware upload had never begun on the OTA service.

### Confirmed partition map

For the local `heltec-v4` 16 MB build:

```text
name     subtype  offset   size
nvs      nvs      0x9000   0x5000
otadata  ota      0xe000   0x2000
app0     ota_0    0x10000  0x640000
app1     ota_1    0x650000 0x640000
spiffs   spiffs   0xc90000 0x360000
coredump coredump 0xFF0000 0x10000
```

The Unified OTA loader belongs in `app1`, which is confirmed at:

```text
0x650000
```

Never assume this address for another target or partition table; verify the local manifest/partition map first.

### Install the Unified OTA loader without wiping settings

Download the ESP32-S3 loader:

```powershell
Invoke-WebRequest `
  -Uri "https://github.com/meshtastic/esp32-unified-ota/releases/latest/download/mt-esp32s3-ota.bin" `
  -OutFile ".\mt-esp32s3-ota.bin"
```

Find PlatformIO esptool:

```powershell
$esptool = Get-ChildItem "$env:USERPROFILE\.platformio\packages\tool-esptoolpy" -Recurse -Filter esptool.py |
    Select-Object -First 1

$esptool.FullName
```

Write **only** `app1`:

```powershell
py "$($esptool.FullName)" `
  --chip esp32s3 `
  --port COM20 `
  --baud 921600 `
  write_flash `
  0x650000 `
  ".\mt-esp32s3-ota.bin"
```

Validated result on 2026-08-26:

```text
Wrote 636544 bytes ... at 0x00650000
Hash of data verified.
```

This preserves the normal application in `app0`, NVS configuration, and filesystem because it writes only the `app1` partition.

Do **not** use `erase_flash` for this repair.

## Diagnostic decision tree for missing cloud data

Use this order; it avoids guessing.

### 1. Confirm field node transmission

On the Seeed serial log, confirm the custom rock packet and/or telemetry packet is actually transmitted over LoRa.

For the sandstone build, the rock packet is `PRIVATE_APP` and the standard environmental temperature packet is `TELEMETRY_APP`.

### 2. Check Neon

If Neon has no new rows, continue upstream rather than assuming a dashboard problem.

### 3. Check Vercel runtime logs

Look specifically for requests to:

```text
POST /api/ingest
```

If the dashboard is polling `/api/readings` but there are **zero POSTs to `/api/ingest`**, the problem is before Vercel: the Heltec gateway is not making upload attempts.

That exact symptom led to discovery of the OLED/TFT compile-gate bug.

### 4. Confirm the gateway module actually compiled

The OLED target contains:

```text
-D HELTEC_V4
-D HELTEC_V4_OLED
```

The TFT target contains:

```text
-D HELTEC_V4
-D HELTEC_V4_TFT
```

Therefore the gateway must be enabled using the common `HELTEC_V4` definition if both models should support it.

### 5. Confirm local secret exists

If the module is compiled but the ingest key is empty, `runOnce()` returns without attempting an upload. Check `src/modules/hobo_gateway_secrets.h` without printing the secret.

### 6. Watch serial output

Serial output distinguishes the remaining cases immediately:

```text
CCA HTTP gateway enabled...
```

means the module exists.

```text
CCA HTTP gateway: queued ROCK...
```

means it is receiving and parsing the mesh packet.

```text
CCA HTTP gateway: INGEST_KEY is empty
```

means local secret configuration is wrong.

```text
CCA HTTP gateway: cloud stored packet ... (HTTP 201)
```

means the Heltec -> Vercel leg succeeded.

An HTTP 401 means the supplied key does not match Vercel production `INGEST_KEY`.

An HTTP 5xx means the request reached Vercel and the server/database side must be inspected.

## Verify normal network access

Known gateway IP during this setup:

```text
192.168.1.147
```

Basic test:

```powershell
py -m meshtastic --host 192.168.1.147 --get device.role
```

If this succeeds, the Heltec is reachable through the normal Meshtastic TCP API.

## TLS note

The current gateway uses `WiFiClientSecure::setInsecure()`. HTTPS traffic is encrypted, but the Heltec does not validate the server certificate chain. The API write is additionally protected by `X-Ingest-Key`. Certificate validation/pinned CA can be added later for a hardened deployment.

## Important operational lessons

1. A successful PlatformIO firmware flash does **not** prove a custom module was compiled into that environment.
2. `4403=True` and `3232=False` after a failed OTA is a useful signature: normal firmware came back, but the OTA listener did not.
3. Verify the partition table before any raw-offset flash.
4. The Unified OTA loader can be repaired independently in `app1`; a factory wipe is unnecessary for this failure.
5. Keep `hobo_gateway_secrets.h` local and git-ignored.
6. Zero Vercel `/api/ingest` POSTs means diagnose the Heltec before debugging Neon/dashboard code.
7. For this physical OLED Heltec V4, use `heltec-v4`, not `heltec-v4-tft`.
