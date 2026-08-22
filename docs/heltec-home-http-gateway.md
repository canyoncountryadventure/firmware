# Heltec Home HTTP Gateway

This branch turns the Heltec WiFi LoRa 32 V4 + TFT into a direct Meshtastic-to-cloud gateway for the Canyon Country environmental monitoring network.

## Data path

```text
HOBO MX2001 / MX2201 / MX2203
        -> field Meshtastic node
        -> LoRa mesh
        -> Heltec Hub (heltec-v4-tft)
        -> HTTPS over home Wi-Fi
        -> https://meshtastic-ecru.vercel.app/api/ingest
        -> Neon PostgreSQL / dashboard
```

No Raspberry Pi or always-on PC is required after the Heltec is flashed and configured.

## What is uploaded

The gateway listens promiscuously for decoded packets from nodes marked **Favorite** in the Heltec NodeDB.

- `PRIVATE_APP` 19-byte `MX` packets are decoded as MX2001 stage + temperature records.
- `TELEMETRY_APP` environment telemetry is decoded as temperature telemetry (MX2201/MX2203 and compatible sensors).
- Radio metadata is included: RSSI, SNR, hop start/limit, hops away, relay node, channel, gateway name and packet ID.
- A small in-memory packet-ID ring prevents normal LoRa duplicate/rebroadcast copies from being uploaded twice.
- HTTPS uploads run on a separate OSThread so the mesh packet handler does not wait on the internet request.
- Failed uploads are retried up to four times. The HOBO remains the authoritative logger during longer internet outages.

## Secret configuration

Never commit the Vercel ingest key.

Copy:

```powershell
Copy-Item src\hobo_gateway_secrets.example.h src\hobo_gateway_secrets.h
notepad src\hobo_gateway_secrets.h
```

Set `HOBO_HTTP_GATEWAY_INGEST_KEY` to the same value as the Vercel project's `INGEST_KEY` environment variable.

`src/hobo_gateway_secrets.h` is ignored by git.

## Build

```powershell
pio run -e heltec-v4-tft
```

The update image is normally:

```text
.pio\build\heltec-v4-tft\firmware.bin
```

## Flash update image

Put the Heltec in the ESP32-S3 ROM bootloader:

1. Hold LEFT/PRG.
2. Tap RIGHT/RST once.
3. Release LEFT/PRG.
4. Identify the new COM port.

Then flash the application update at `0x10000`:

```powershell
py -m esptool --port COM20 write-flash 0x10000 .pio\build\heltec-v4-tft\firmware.bin
```

Use the actual COM port if it is not COM20. Press RIGHT/RST once after flashing if the board does not reboot automatically.

## Authorize field nodes

The gateway defaults to `HOBO_HTTP_GATEWAY_FAVORITES_ONLY=1` so unrelated public LongFast telemetry is not written to the private database.

Example:

```powershell
py -m meshtastic --host 192.168.1.147 --set-favorite-node !5e021e35
```

Repeat for each owned monitoring node.

## Verification

Watch the Heltec serial log after a fresh field measurement. Expected messages include:

```text
HOBO HTTP gateway: queued ...
HOBO HTTP gateway: cloud stored packet ... (HTTP 201)
```

Once direct HTTPS ingestion is verified, the normal Meshtastic MQTT module can be disabled if it is no longer wanted.

## TLS note

The first field build uses `WiFiClientSecure::setInsecure()`: HTTPS traffic is encrypted, but the server certificate chain is not validated by the Heltec. API writes are still protected by the `X-Ingest-Key` application secret. A pinned CA/certificate-validation upgrade should be done before treating the gateway as hardened production infrastructure.
