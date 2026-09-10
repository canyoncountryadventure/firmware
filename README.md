# Fucking Around — Experimental Heltec Water-Alert Gateway

> **Branch:** `fucking-around-heltec`
>
> **Status:** Experimental/test gateway. **Do not treat this as the canonical production Heltec branch.**
>
> **Canonical production gateway:** `cca-heltec-sensor-gateway`
>
> **Hardware:** Heltec WiFi LoRa 32 V4 OLED
>
> **PlatformIO target:** `heltec-v4`

This branch pairs with the experimental `fucking-around` Seeed water-level branch. Its purpose is to receive experimental Meshtastic water alerts at the home Heltec and push those alerts into GitHub while preserving the older MX2001-to-cloud gateway path used for testing.

## What this branch handles

The gateway currently recognizes two custom inputs from remote Meshtastic nodes:

| Input | Meshtastic port | Action |
|---|---|---|
| MX2001 19-byte `MX` packet | `PRIVATE_APP` | Decode and upload stage/temperature/logger/radio metadata through the existing HTTP ingest path |
| `WATER_ALERT|...` text | `TEXT_MESSAGE_APP` | Create a GitHub issue so normal GitHub notifications can provide the alert email path |

All other packet types are ignored by this experimental gateway module.

## Paired experimental field node

Use with:

```text
fucking-around
```

That Seeed branch currently tests the DFRobot A02YYUW / SEN0311 waterproof ultrasonic sensor and emits threshold/fault messages beginning with:

```text
WATER_ALERT|
```

The field-node and gateway branches are deliberately separate from production so the water-alert experiment cannot silently change the normal HOBO deployment line.

## Tested alert path

```text
A02YYUW ultrasonic sensor
    ↓ UART
Seeed XIAO + Wio-SX1262 (`fucking-around`)
    ↓ Meshtastic WATER_ALERT|...
mesh / relays
    ↓
Heltec V4 OLED (`fucking-around-heltec`)
    ↓ Wi-Fi
GitHub issue
    ↓
GitHub notification/email
```

The GitHub issue records the alert text plus available source-node and RF metadata such as packet ID, RSSI, SNR, hop information, and station name.

## Secrets required for a deployable build

The GitHub Actions workflow is:

```text
.github/workflows/build_fucking_around_heltec.yml
```

It expects these repository secrets:

```text
HOBO_HTTP_GATEWAY_INGEST_KEY
WATER_GITHUB_TOKEN
```

The workflow intentionally refuses to publish a deployable artifact if `WATER_GITHUB_TOKEN` is missing. It may compile with a placeholder only to prove the source builds, then fails the job so that placeholder firmware is not accidentally deployed.

The build injects the secrets into a generated local header during CI. Do not hard-code real credentials into repository source.

## Build target

The physical gateway is the OLED Heltec V4:

```text
heltec-v4
```

Do not use `heltec-v4-tft` for this unit.

Local build, if needed:

```powershell
py -m platformio run -e heltec-v4
```

The preferred repeatable build path is GitHub Actions so the protected secrets are injected without being committed.

## Current MX2001 compatibility path

The branch also retains the older MX2001 HTTP gateway behavior:

```text
HOBO MX2001
   ↓ BLE
field Meshtastic node
   ↓ PRIVATE_APP / 19-byte MX packet
Heltec V4
   ↓ Wi-Fi / HTTPS
Vercel ingest
   ↓
Neon
```

That compatibility path is useful during the experiment, but new production multi-station gateway development belongs on `cca-heltec-sensor-gateway`, not here.

## Branch rules

1. `fucking-around-heltec` is experimental only.
2. Keep it paired with experimental `fucking-around` water-node work.
3. Do not promote a water-alert change to production until the full field-node → mesh → Heltec → alert path is tested.
4. Do not commit `HOBO_HTTP_GATEWAY_INGEST_KEY` or `WATER_GITHUB_TOKEN`.
5. Do not publish an artifact built with placeholder credentials.
6. Preserve ordinary Meshtastic radio/Wi-Fi behavior while testing gateway additions.
7. Production Heltec changes belong on `cca-heltec-sensor-gateway`.

## Related branches

| Branch | Purpose |
|---|---|
| `fucking-around` | Experimental Seeed ultrasonic/water field node |
| `fucking-around-heltec` | This experimental water-alert gateway |
| `cca-heltec-sensor-gateway` | Canonical production Heltec sensor gateway |
| `hobo-mx2001-mx2201-mx2203` | Canonical production HOBO field-node firmware |

This README is the operating front page for the experimental Heltec branch; older `heltec-home-http-gateway*` branches are historical and should not be mistaken for the current production gateway.
