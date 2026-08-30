# Universal HOBO Reader: MX2001 + MX2201 + MX2203

This directory contains the shared HOBO BLE protocol implementation plus the RAK4631 deployment wrapper used by branch `hobo-mx2001-mx2201-mx2203-rak4631`.

## Supported logger data

- **MX2001:** live water level/stage + temperature
- **MX2201:** live temperature
- **MX2203:** live temperature

## RAK4631 behavior

On the RAK branch, `HOBOMX2001MX2201MX2203TelemetryRAK.cpp` wraps the universal reader and adds automatic mesh publishing:

1. Discover and connect to one supported HOBO over BLE.
2. Perform the normal startup live read.
3. Publish the startup reading as standard Meshtastic `TELEMETRY_APP` environmental telemetry.
4. Perform another live `NEWREAD64` read every 60 minutes by default.
5. Publish each successful automatic reading to the mesh.
6. Retry a failed automatic read after 60 seconds.

The automatic interval can be overridden at build time with `CCA_HOBO_AUTO_READ_INTERVAL_MS`.

Temperature is transmitted in Celsius. For MX2001, stage is transmitted in the environmental telemetry `distance` field in millimetres.

There is intentionally **no custom PIR or trail-counter integration** in the RAK HOBO branch.

## Manual Meshtastic command

A direct message remains available for diagnostics and trigger-driven deployments:

```text
READ
```

The bridge performs one fresh BLE `NEWREAD64` read and replies directly to the requester. A manual `READ` does not create a duplicate automatic telemetry packet.

## CCA station modes

- **Hidden Valley:** automatic remote HOBO telemetry.
- **Home:** automatic local HOBO reads on the Heltec gateway.
- **Fishlake:** Heltec-triggered/polled `READ`; Fishlake is not intended to free-run automatic transmissions.

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

The MX2001 response arrives in the two hardware-proven fragments used by the prior combined reader. Temperature is decoded from bytes 17-18 of fragment 1. Water level is the big-endian float beginning at byte 3 of fragment 2 and is converted from meters to feet internally; the RAK mesh wrapper publishes stage in millimetres.

## Discovery and fallback

The reader discovers Onset candidates dynamically from manufacturer data, the common HOBO service UUID, or a HOBO/MX local name. No logger MAC is hard-coded.

The first model probe is:

1. `INIT`
2. direct `NEWREAD64`

This directly identifies MX2201 and MX2203 and normally identifies MX2001. For older MX2001/MX2201 behavior that requires metadata setup, the proven MX2001 and MX2201 metadata fallback commands are retained.

A positively identified MX2203 advertisement does not receive MX2001/MX2201 metadata fallback commands.

## Code provenance

- MX2001 + MX2201 behavior is based on the hardware-proven combined reader.
- MX2203 response parsing is based on the hardware-proven MX2203 reader.
- MX2203 temperature conversion comes from HOBOconnect `OnsetSDK.dll`, not a fitted field equation.
- The RAK wrapper changes scheduling and mesh publication only; it does not replace the shared logger decoder.
