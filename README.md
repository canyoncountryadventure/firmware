# HOBO MX2001 → Meshtastic

**Production branch:** `hobo-mx2001`  
**Logger:** Onset HOBO MX2001  
**Proven hardware:** RAK19003 + RAK4631  
**Status:** Finalized working water-level + temperature build

This branch is the long-lived MX2001 production branch. Branch names are logger-centric; board names and words such as `integration`, `test`, or `diagnostic` are not used for production branch naming.

## Proven behavior

- automatic MX2001 BLE discovery and connection
- live `NEWREAD64` water-level + temperature acquisition
- direct Meshtastic `READ` command and reply
- automatic compact PRIVATE_APP telemetry for logger records
- normal Meshtastic relay/rebroadcast behavior
- tracked Windows receiver at `tools/mx2001_receiver.py`

Example direct-message reply:

```text
Level: 1.04 ft
Temp: 78.9 F
```

## Build

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e rak4631
```

## Production branch map

| Logger/build | Branch |
|---|---|
| MX2001 | `hobo-mx2001` |
| MX2201 | `hobo-mx2201` |
| Combined MX2201 + MX2001 Seeed reader | `hobo-mx2201-mx2001` |
| MX2203 | `hobo-mx2203` |

## Archived names

These remain only for Git history and should not be used for new work:

- `mx2001-integration` → superseded by `hobo-mx2001`
- `mx2201-integration` → superseded by `hobo-mx2201`
- `mx2201-newread-test` → historical protocol test
- `mx2203-discovery-test` → historical MX2203 discovery/calibration work
- `hobo-universal-test` → historical combined-reader bench work

## Source naming note

The working MX2001 implementation still contains historical filenames such as `MX2001Diagnostic.cpp`. Those filenames are retained intentionally because that exact implementation was physically validated. Production **branch and documentation naming** is now standardized without changing proven firmware solely for cosmetic reasons.

The historical `MX2001_INTEGRATION.md` document is retained as a technical record; it does not define the current branch name.
