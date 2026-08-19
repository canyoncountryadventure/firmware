# HOBO MX2201 + MX2001 → Meshtastic

**Production branch:** `hobo-mx2201-mx2001`  
**Target:** Seeed XIAO nRF52840 + Wio-SX1262  
**Status:** Stable combined on-demand reader

This branch contains the hardware-proven combined Seeed reader for:

- **MX2201** — temperature
- **MX2001** — water level + temperature

Send a direct Meshtastic message:

```text
READ
```

The node performs a fresh BLE logger read and replies directly to the requester. There is no automatic periodic HOBO telemetry in this combined build.

## Production branch map

| Logger/build | Branch |
|---|---|
| MX2001 | `hobo-mx2001` |
| MX2201 | `hobo-mx2201` |
| Combined MX2201 + MX2001 | `hobo-mx2201-mx2001` |
| MX2203 | `hobo-mx2203` |

## Archived development branches

- `mx2001-integration` → old name for MX2001 production history
- `mx2201-integration` → old name for MX2201 production history
- `mx2201-newread-test` → historical MX2201 protocol testing
- `mx2203-discovery-test` → historical MX2203 discovery/calibration work
- `hobo-universal-test` → historical combined-reader bench branch

The production-facing combined module is documented under:

```text
src/modules/Telemetry/HOBOMX2201MX2001/
```

Historical bench implementation files remain in Git history and are not the naming standard for current branches.
