# Archived HOBO Development Work

This directory documents superseded HOBO/Meshtastic development branches and test code. These branches are retained for protocol history and rollback only.

## Production branches

Use these for current work:

- `hobo-mx2001` — MX2001 recovery/reference build
- `hobo-mx2201` — MX2201 recovery/reference build
- `hobo-mx2203` — MX2203 recovery/reference build
- `hobo-mx2201-mx2001` — combined MX2201 + MX2001 recovery/reference build
- `hobo-mx2001-mx2201-mx2203` — **canonical universal production build for Seeed and RAK4631**

## Archived branches

Do not deploy new nodes from these:

- `mx2001-integration` — superseded by `hobo-mx2001`
- `mx2201-integration` — superseded by `hobo-mx2201`
- `mx2201-newread-test` — historical MX2201 protocol testing
- `mx2203-discovery-test` — historical MX2203 discovery/calibration testing
- `hobo-universal-test` — historical combined-reader development
- `hobo-mx2001-mx2201-mx2203-rak4631` — completed RAK4631 validation branch; folded into the canonical universal branch on 2026-08-19
- `rak4631-mx2201-raw-debug` — temporary MX2201 raw NEWREAD64 diagnostic branch used to investigate the apparent 108.4 F reading; investigation showed the reading came from a different hot attic logger

## Archived source names

Some historical implementation names remain in Git history or as tiny compatibility routers. They are not separate production logger implementations on the universal branch.

The production three-model source is:

```text
src/modules/Telemetry/HOBOMX2001MX2201MX2203/
```

The canonical branch supports both Seeed XIAO nRF52840 + Wio-SX1262 and RAK4631 / RAK19003.

Do not restore older test folders into the active build unless intentionally performing protocol research.

## Recovery rule

Do not delete the archived branches simply to make the branch list shorter. They preserve tested milestones and reverse-engineering history. New field deployments should use `hobo-mx2001-mx2201-mx2203`.
