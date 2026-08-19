# Archived HOBO Development Work

This directory documents superseded HOBO/Meshtastic development branches and test code. These branches are retained for protocol history and rollback only.

## Production branches

Use these for current work:

- `hobo-mx2001` — MX2001
- `hobo-mx2201` — MX2201
- `hobo-mx2203` — MX2203
- `hobo-mx2201-mx2001` — combined MX2201 + MX2001
- `hobo-mx2001-mx2201-mx2203` — **recommended universal three-model build**

## Archived branches

Do not deploy new nodes from these:

- `mx2001-integration` — superseded by `hobo-mx2001`
- `mx2201-integration` — superseded by `hobo-mx2201`
- `mx2201-newread-test` — historical MX2201 protocol testing
- `mx2203-discovery-test` — historical MX2203 discovery/calibration testing
- `hobo-universal-test` — historical combined-reader development

## Archived source names

Some historical implementation names remain in Git history or as tiny compatibility routers. They are not the production implementation on the universal branch.

The production three-model source is:

```text
src/modules/Telemetry/HOBOMX2001MX2201MX2203/
```

Do not restore older test folders into the active build unless intentionally performing protocol research.

## Recovery rule

Do not delete the archived branches simply to make the branch list shorter. They preserve tested milestones and reverse-engineering history. Production work should occur only on the `hobo-*` branches listed above.
