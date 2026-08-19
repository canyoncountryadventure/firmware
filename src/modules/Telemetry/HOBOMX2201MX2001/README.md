# HOBO MX2201 + MX2001 Combined Reader

Finalized Seeed XIAO nRF52840 integration for the two hardware-tested Onset HOBO logger models:

- **MX2201** — live temperature
- **MX2001** — live water level and temperature

This module is intentionally scoped to these two models. Future HOBO logger integrations should be developed separately and only merged here if explicitly desired after hardware validation.

## Hardware target

- Seeed XIAO nRF52840
- Wio-SX1262 / Meshtastic US915 build

## Validated behavior

The same firmware image has been hardware-tested with both physical logger models and can:

1. discover an Onset HOBO without a hard-coded MAC address;
2. connect as a BLE central while Meshtastic Bluetooth remains available;
3. distinguish MX2201 from MX2001 from the live protocol response;
4. read MX2201 temperature;
5. read MX2001 water level and temperature;
6. accept a direct Meshtastic text message of `READ` and return a fresh measurement;
7. disconnect, rescan, and switch between MX2201 and MX2001 without reflashing;
8. recover from transient HOBO service/characteristic/notification discovery failures.

## Discovery

A BLE advertisement is treated as a HOBO candidate when at least one of the following is present:

- Onset manufacturer data beginning `C5 00`;
- the known HOBO 128-bit BLE service UUID;
- a local BLE name containing `HOBO`, `MX2201`, or `MX2001`.

The logger MAC is learned dynamically. It is not compiled into the firmware.

The 22-byte Onset manufacturer payload is used only as an MX2001 hint. The live `NEWREAD64` response is authoritative for model identification.

## Common live-read command

```text
01 01 08 04 04 00 00 00 00 00 00
```

## MX2201 response

Expected response prefix:

```text
01 01 07 04 04 00 04 04
```

Bytes 8-11 contain the big-endian raw temperature value. The bench-proven MX2201 calibration is retained.

Expected Meshtastic `READ` reply:

```text
MX2201
Temp: 70.5 F
```

## MX2001 response

The live response is split across two known notification fragments. Temperature is decoded from fragment 1. Water level is decoded from the big-endian float in fragment 2 and converted from meters to feet.

Expected Meshtastic `READ` reply:

```text
MX2001
Level: 0.91 ft
Temp: 75.4 F
```

## Connection recovery

After BLE connection, the reader waits briefly before discovery and retries service, characteristic, and notification setup up to three times.

A transient BLE discovery failure places that logger on a short **5-second** cooldown before it may be attempted again. A device that connects successfully but fails both the MX2201 and MX2001 protocol probes receives the longer **60-second** unsupported-candidate cooldown.

## Probe sequence

1. Send `INIT`.
2. Try `NEWREAD64` directly.
3. If it times out, try the metadata profile suggested by the advertisement and retry `NEWREAD64`.
4. If that still times out, try the other known model profile and retry.
5. Accept only a valid MX2201 or MX2001 live-response signature.

## Meshtastic command

From another Meshtastic node, send a direct message to the Seeed containing:

```text
READ
```

`/READ` is also accepted.

The response is generated from a fresh BLE live read rather than a hard-coded or stale value.

## Build

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e seeed_xiao_nrf52840_kit
```

Upload:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e seeed_xiao_nrf52840_kit -t upload
```

## Branch

Production combined branch:

```text
hobo-mx2201-mx2001
```

The earlier `hobo-universal-test` branch is retained as the bench-test history that established the working protocol behavior.

## Current telemetry scope

This finalized combined reader preserves the exact behavior that was hardware-tested: discovery, model identification, live BLE measurements, reconnection/model switching, and Meshtastic `READ` request/reply.

Automatic scheduled environmental telemetry broadcasting is not added in this finalization commit because that behavior was not part of the combined hardware test. It can be layered onto this reader separately without changing the proven MX2201/MX2001 BLE protocol core.
