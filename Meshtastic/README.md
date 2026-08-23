# Meshtastic Monitoring Firmware

This folder is the user-facing project structure for the custom Meshtastic monitoring firmware.

## Branches to use

### Field HOBO radios

**Production branch:**

```text
hobo-mx2001-mx2201-mx2203
```

**Frozen hardware-validated snapshot:**

```text
hobo-universal-validated-2026-08-19
```

These branches support Seeed XIAO nRF52840 + Wio-SX1262 and RAK4631 / RAK19003 field radios reading HOBO loggers over BLE.

### Home internet gateway

**Production branch:**

```text
heltec-home-http-gateway
```

**Hardware:** Heltec WiFi LoRa 32 V4 + TFT  
**PlatformIO target:** `heltec-v4-tft`

This branch receives the custom MX2001 mesh packet and uploads it directly over Wi-Fi/HTTPS to the Vercel ingest API and Neon PostgreSQL. The end-to-end MX2001 path was validated on 2026-08-22.

## Project layout

```text
Meshtastic/
├── README.md
├── SEEED-XIAO/
│   ├── README.md
│   ├── build.ps1
│   └── flash.ps1
├── RAK4631/
│   ├── README.md
│   ├── build.ps1
│   └── flash.ps1
├── HELTEC-HOME/
│   └── README.md
├── SHARED-HOBO/
│   └── README.md
└── ARCHIVE/
    └── README.md
```

## Field radio guides

### Seeed XIAO nRF52840 + Wio-SX1262

Guide: [`SEEED-XIAO/README.md`](SEEED-XIAO/README.md)

PlatformIO target:

```text
seeed_xiao_nrf52840_kit
```

### RAK4631 / RAK19003

Guide: [`RAK4631/README.md`](RAK4631/README.md)

PlatformIO target:

```text
rak4631
```

### Heltec V4 home gateway

Guide: [`HELTEC-HOME/README.md`](HELTEC-HOME/README.md)

PlatformIO target:

```text
heltec-v4-tft
```

## HOBO field-node behavior

The field firmware supports:

| Logger | Automatic telemetry | Direct `READ` |
|---|---|---|
| MX2001 | Water level + temperature | Water level + temperature |
| MX2201 | Temperature | Temperature |
| MX2203 | Temperature | Temperature |

Automatic telemetry is tied to the HOBO logger's own write pointer and configured logging interval. The radio waits for a confirmed new logger record, performs one fresh read, then queues one Meshtastic packet.

Direct commands to a field radio:

```text
LOGGER
READ
LOCK
UNLOCK
```

- `LOGGER` — model, logger MAC, BLE RSSI, detected logging interval and lock state.
- `READ` — immediate fresh measurement without disturbing the automatic schedule.
- `LOCK` — persistently assign the radio to the currently identified HOBO BLE MAC.
- `UNLOCK` — clear that assignment and resume discovery.

See [`SHARED-HOBO/README.md`](SHARED-HOBO/README.md) for protocol and scheduler details.

## MX2001 cloud path

The finalized Heltec home-gateway build accepts only the custom MX2001 packet format:

```text
HOBO MX2001
   -> BLE
Field radio
   -> PRIVATE_APP "MX..."
Meshtastic mesh
   ->
Heltec Home
   -> HTTPS
Vercel ingest API
   ->
Neon PostgreSQL
```

Normal environmental telemetry from neighboring Meshtastic nodes is not uploaded by the Heltec gateway. Favorites are not required for MX2001 ingestion; the custom packet format itself is the filter.

## Old branches

Old model-specific, discovery, integration, recovery and raw-debug branches are retained only for rollback/protocol history. Do not use them for normal deployment. See [`ARCHIVE/README.md`](ARCHIVE/README.md).