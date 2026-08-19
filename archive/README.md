# HOBO Firmware Archive Index

This directory documents historical branch and source names that are preserved in Git history but are not current production naming.

## Current production branches

- `hobo-mx2001`
- `hobo-mx2201`
- `hobo-mx2201-mx2001`
- `hobo-mx2203`

## Archived branches

- `mx2001-integration` — superseded by `hobo-mx2001`
- `mx2201-integration` — superseded by `hobo-mx2201`
- `mx2201-newread-test` — historical MX2201 protocol testing
- `mx2203-discovery-test` — historical MX2203 BLE discovery and calibration
- `hobo-universal-test` — historical combined MX2201/MX2001 bench work

Each archived branch has an `ARCHIVED` README and should not be used for deployment.

## Archived source folders

`src/modules/Telemetry/HOBOUniversalTest/` was inherited by the MX2203 production branch from earlier combined-reader development. It is not used by the final MX2203 build and has been removed from `hobo-mx2203`. Its complete contents remain recoverable from Git history and the archived `hobo-universal-test` branch.

`src/modules/Telemetry/HOBOMX2201MX2001/` remains on `hobo-mx2203` only as a compatibility router required by the existing Meshtastic module hook. The actual MX2203 implementation is exclusively in:

```text
src/modules/Telemetry/HOBOMX2203/
```

Do not add new MX2203 implementation code to the compatibility router.
