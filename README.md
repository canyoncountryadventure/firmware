# HOBO MX2201 → Meshtastic

**Production branch:** `hobo-mx2201`  
**Logger:** Onset HOBO MX2201  
**Proven hardware:** Seeed XIAO nRF52840 + Wio-SX1262  
**Status:** Stable temperature build

This branch is the long-lived MX2201 production branch. Production branches are named for the HOBO logger model, not with suffixes such as `integration`, `test`, or board names.

## Proven behavior

- automatic MX2201 BLE discovery and connection
- live temperature acquisition with Onset `NEWREAD64`
- standard Meshtastic environmental temperature telemetry
- direct live reads before scheduled transmissions
- stable one-hour logger interval testing

## Recovery points

Stable tags remain immutable recovery points:

```text
mx2201-stable-newread-2026-08-13
mx2201-stable-2026-08-13
```

## Production branch map

| Logger/build | Branch |
|---|---|
| MX2001 | `hobo-mx2001` |
| MX2201 | `hobo-mx2201` |
| Combined MX2201 + MX2001 Seeed reader | `hobo-mx2201-mx2001` |
| MX2203 | `hobo-mx2203` |

## Archived names

Do not use these for new development or deployment:

- `mx2201-integration` → superseded by `hobo-mx2201`
- `mx2001-integration` → superseded by `hobo-mx2001`
- `mx2201-newread-test` → historical protocol testing
- `mx2203-discovery-test` → historical MX2203 discovery/calibration work
- `hobo-universal-test` → historical combined-reader bench work

Historical technical documents may still contain the word `INTEGRATION`; those filenames are preserved as records and do not define current production naming.

## Build

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e seeed_xiao_nrf52840_kit
```
