# CCA Firmware Branch Map

This file is the branch-status source of truth.

## Current branches

| Branch | Status | Purpose |
|---|---|---|
| `cca-heltec-sensor-gateway` | **CURRENT — HELTEC** | Canonical Heltec V4 OLED sensor gateway, Mesh -> Vercel/Neon, future direct HOBO BLE integration |
| `hobo-mx2001-mx2201-mx2203` | **CURRENT — HOBO FIELD NODES** | Proven universal HOBO reader for Seeed XIAO nRF52840 and RAK4631, including automatic record-aligned telemetry and DM commands |
| `hobo-universal-validated-2026-08-19` | FROZEN REFERENCE | Hardware-validated universal HOBO snapshot |

## Historical / experimental branches

These branches are retained only as source history. Do not use them as the starting point for new Heltec work:

- `heltec-home-http-gateway`
- `heltec-home-http-gateway-hidden-valley`
- `heltec-home-http-gateway-rock`
- `CCA-MX-HOBO-PIR-ROCK-SEEED-v1`
- `CCA-MX-HOBO-PIR-SEEED-v1`
- `CCA-MX-HOBO-PIR-SEEED-v1-ci`
- `hobo-mx2001`
- `hobo-mx2201`
- `hobo-mx2203`
- `hobo-mx2201-mx2001`
- `mx2001-integration`
- `mx2201-integration`
- `mx2201-newread-test`
- `mx2203-discovery-test`
- `rak4631-mx2201-raw-debug`
- `trail-sen0171`

## Rules going forward

- Heltec gateway changes go only to `cca-heltec-sensor-gateway`.
- Do not create deployment-location branches.
- Do not create a new gateway branch for each sensor type.
- New field-sensor functionality should be integrated into a deliberate field-node production line rather than another throwaway branch.
- Experimental branches should be short-lived and clearly prefixed `test/` or `exp/` if new experiments are unavoidable.
- Stable milestones should be tagged or documented as validated snapshots rather than copied into multiple synonymous branches.

## Naming

User-facing firmware, branch names, workflows, and documentation should describe function rather than a particular test material or deployment location. Legacy packet identifiers may remain internally when required for backward compatibility with deployed nodes and database schemas.
