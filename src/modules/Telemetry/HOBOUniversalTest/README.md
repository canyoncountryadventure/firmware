# MX2201 + MX2001 Proven BLE Implementation Core

This directory contains the hardware-proven BLE/protocol implementation that was developed on the `hobo-universal-test` branch.

On the finalized `hobo-mx2201-mx2001` branch, the production-facing entrypoint is:

```text
src/modules/Telemetry/HOBOMX2201MX2001/HOBOMX2201MX2001Telemetry.h
```

The implementation source is intentionally retained here without a large rename/refactor so the exact code that passed the MX2201/MX2001 hardware tests remains unchanged.

## Supported models on this branch

- HOBO MX2201: live temperature
- HOBO MX2001: live water level + temperature

Future HOBO logger models should be developed in a separate module/branch first rather than added implicitly to this pair.

## Proven features

- Dynamic Onset/HOBO BLE discovery with no hard-coded logger MAC
- MX2201/MX2001 protocol identification
- Shared `NEWREAD64` live-read request
- MX2201 temperature decoding
- MX2001 temperature + level decoding
- Direct Meshtastic `READ` and `/READ` command handling
- Automatic reconnect and model switching
- Three-attempt BLE service/characteristic/notify discovery
- 5-second retry for transient BLE discovery failures
- 60-second cooldown for candidates that fail both supported protocol probes

See `../HOBOMX2201MX2001/README.md` for the finalized combined-module documentation.
