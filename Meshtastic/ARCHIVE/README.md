# Archived / Recovery Branches

For normal field work, ignore the old branches and use only:

```text
hobo-mx2001-mx2201-mx2203
```

That branch now contains the production Seeed and RAK4631 universal firmware.

## Model-specific recovery/reference branches

These are retained because they preserve known working milestones:

```text
hobo-mx2001
hobo-mx2201
hobo-mx2203
hobo-mx2201-mx2001
```

Do not use them for a new universal deployment unless intentionally rolling back or debugging one logger model.

## Historical development branches

These are protocol/development history, not current deployment branches:

```text
mx2001-integration
mx2201-integration
mx2201-newread-test
mx2203-discovery-test
hobo-universal-test
hobo-mx2001-mx2201-mx2203-rak4631
rak4631-mx2201-raw-debug
```

The RAK validation branch was successfully tested and its working code was folded into the canonical production branch on 2026-08-19.

The RAK raw-debug branch existed only to investigate an apparent 108.4 F MX2201 reading. That reading was later confirmed to be a legitimate reading from a second MX2201 near a hot attic area, not a decoder failure.

## Rule

Do not delete these branches simply to make the GitHub branch menu shorter. They preserve rollback points and reverse-engineering history.

For day-to-day use, stay on:

```text
hobo-mx2001-mx2201-mx2203
```
