# Universal HOBO Reader: MX2001 + MX2201 + MX2203

This is the production implementation used by branch `hobo-mx2001-mx2201-mx2203`.

## Supported hardware

- Seeed XIAO nRF52840 + Wio-SX1262
- RAK4631 / RAK19003

Both targets were hardware-validated with MX2001, MX2201, and MX2203 on 2026-08-19.

## Supported logger data

- **MX2001:** live water level + temperature
- **MX2201:** live temperature
- **MX2203:** live temperature

## Automatic telemetry

After connecting and identifying the HOBO, the bridge takes one startup reading and transmits it automatically.

It then uses the common Onset `STATUS` request to read the logger write pointer and logging interval. The radio checks near the next expected logger record, and a write-pointer change triggers exactly one fresh `NEWREAD64` read and one Meshtastic transmission.

- **MX2001:** sends the existing 19-byte `PRIVATE_APP` packet containing water level, temperature, logger MAC, sequence, and BLE RSSI. This remains compatible with `tools/mx2001_receiver.py` and the water dashboard ingest path.
- **MX2201:** sends standard Meshtastic environmental temperature telemetry on `TELEMETRY_APP`.
- **MX2203:** sends standard Meshtastic environmental temperature telemetry on `TELEMETRY_APP`.

A direct manual `READ` does not consume the logger write pointer or reset the automatic schedule.

If repeated `STATUS` requests fail, the bridge falls back to periodic fresh `NEWREAD64` reads. If the logger interval was learned before the failure, that interval is retained; otherwise the fallback is 60 seconds.

## Meshtastic commands

Commands are direct messages to the radio. A leading slash is optional.

### `READ`

Performs one fresh BLE `NEWREAD64` read and replies directly to the requester. The reply includes:

- HOBO model
- full BLE MAC address
- BLE RSSI captured when the radio selected the logger
- current measurement

Example:

```text
MX2001
Logger: F1:0D:9D:29:C3:2D
BLE: -63 dBm
Level: 0.91 ft
Temp: 75.4 F
```

### `LOGGER`

Reports which physical HOBO the radio is currently locked to without taking a measurement.

Example:

```text
HOBO CONNECTED
Model: MX2203
MAC: F1:0D:9D:29:C3:2D
BLE: -63 dBm
Interval: 10 sec
```

This is intended for bench setups where several radios and several HOBOs are powered at the same time and discovery order determines which logger each radio selects.

## Shared Onset BLE protocol

Service UUID:

```text
CFCBE6BC-CC83-49AC-4146-4EED4F6EE165
```

Command characteristic:

```text
CFCBE6BC-CC83-49AC-4146-4EED4F6FE165
```

INIT:

```text
01 01 04 05 1C 01 00
```

NEWREAD64:

```text
01 01 08 04 04 00 00 00 00 00 00
```

STATUS:

```text
01 01 08 04 05 00 00 00 00 00 00
```

The status response supplies the logger write pointer and logging interval used by automatic telemetry scheduling.

## Live response identification

### MX2201

```text
01 01 07 04 04 00 04 04 [TEMP32 BE] ...
```

### MX2203

```text
01 01 0B 04 04 00 04 04 [TEMP32 BE] ...
```

MX2203 uses the OnsetSDK `TempSensor2F` 14-bit conversion documented in `ONSETSDK.md`.

### MX2001

The MX2001 response arrives in the two hardware-proven fragments used by the prior combined reader. Temperature is decoded from bytes 17-18 of fragment 1. Water level is the big-endian float beginning at byte 3 of fragment 2 and is converted from meters to feet.

## Discovery and fallback

The reader discovers Onset candidates dynamically from manufacturer data, the common HOBO service UUID, or a HOBO/MX local name. No production logger MAC is hard-coded.

The first model probe is always:

1. `INIT`
2. direct `NEWREAD64`

This directly identifies MX2201 and MX2203 and normally identifies MX2001. For older MX2001/MX2201 behavior that requires metadata setup, the proven MX2001 and MX2201 metadata fallback commands are retained.

A positively identified MX2203 advertisement does not receive MX2001/MX2201 metadata fallback commands.

## Board integration

`HOBOMX2001MX2201MX2203Telemetry.cpp` is the shared implementation used by the Seeed build.

`HOBOMX2001MX2201MX2203TelemetryRAK.cpp` is the RAK4631 compile adapter that reuses that exact implementation after the shared dependencies are loaded under the real `RAK_4631` configuration.

The RAK Meshtastic module hook still uses the historical `MX2001Diagnostic` include name, but on the canonical universal branch that header is only an alias to `HOBOMX2001MX2201MX2203TelemetryModule`. The old MX2001-only RAK implementation is not compiled on this branch.

## BLE connection model

The nRF52 Bluetooth layer allocates:

- one BLE peripheral link for a Meshtastic phone connection
- one BLE central link for the HOBO logger

The HOBO reader intentionally maintains only one logger BLE connection at a time. If multiple valid HOBOs are nearby, candidate selection is discovery-order dependent. Use the `LOGGER` command to confirm which physical logger was selected.

## Code provenance

- MX2001 automatic write-pointer scheduling and `PRIVATE_APP` packet behavior come from the hardware-proven MX2001 production sender.
- MX2201 automatic telemetry behavior follows the earlier hardware-proven interval-aligned telemetry implementation.
- MX2001 + MX2201 live BLE behavior is based on the hardware-proven combined reader.
- MX2203 response parsing is based on the hardware-proven MX2203 reader.
- MX2203 temperature conversion comes from HOBOconnect `OnsetSDK.dll`, not a fitted field equation.
- RAK4631 universal BLE behavior was physically validated before being folded into the canonical branch.
