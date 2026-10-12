# RAK water distance + HOBO v5

Based on v4 commit `42af584eaff4f72fdbb99c64ac8ed1339375e78c`.

## Audit evidence

The Cottonwood logs show repeated GATT service-discovery failures, successful DM reception/decryption/reply transmission, and an eventual hardware watchdog reset. After that reset, the same logger connected and was identified as MX2001. This establishes a recovery problem; it does not establish the underlying RF or GATT trigger. Pack Creek's specific failure still needs its own log.

V4 did not count discovery failures toward recovery, relied on the scanner's cached running flag, had unbounded initialization-write retries, and disabled acknowledgment/retry for command replies on this branch.

## Changes

- Service and characteristic discovery and notification setup execute in bounded stages outside the BLE connect callback. Every exhausted stage enters recovery.
- Discovery responses log GATT status and service count. LOGGER includes the last error.
- Failed scan resumes request a checked scan restart. Idle scanning is forcibly restarted every 30 seconds even if the cached running flag is stale.
- Connection requests, cancellation, and disconnect recovery are checked. A stuck disconnect or failed cancellation escalates to the independent hardware watchdog.
- Five failures spanning at least two minutes escalate to a hardware watchdog reset. An absent logger does not trigger resets solely because no advertisement was received. Retry backoff is five seconds.
- GATT setup has a 45-second deadline; initialization through the first successful STATUS baseline has a 30-second deadline.
- Acquisition scanning uses a 50% receive window while disconnected. This increases power use while seeking a logger; scanning stops for connection attempts and connected operation.
- LOGGER reports a verified MX2001, MX2201, or MX2203 model. Unidentified candidates are never reported as a verified model. Last verified model and address persist in a separate checksummed identity record and are explicitly labeled as historical while disconnected.
- LOGGER, READ, system, and water command replies request acknowledgment and retries.
- `LOCK AA:BB:CC:DD:EE:FF` can persist a target before connection. Existing bare LOCK still locks an identified connected logger. UNLOCK remains available.
- Non-connectable advertisements are ignored.

## Builds and compatibility

| Target | Fresh-install/reset sensor default |
| --- | --- |
| `rak4631_sen0313_v5` | DFRobot SEN0313 / A01NYUB |
| `rak4631_mb7388_v5` | MaxBotix MB7388 |

Existing saved sensor selection, calibration, measurement interval, logger lock, node identity, encryption keys, and channels are retained. Build defaults do not overwrite saved sensor selection. The distance configuration and cloud packet formats are unchanged. The MB7388 cold-start settling fix and 12-hour preventive reboot are retained.

Automatic MX2001 transmission remains gated on a new logger recording. A successful initial probe does not create a synthetic hourly record.

## Validation

`python3 test/test_hobo_v5/test_hobo_v5.py` compiles actual firmware recovery, discovery-stage, and LOGGER code with fault injection. Cases cover stale scanner flags, failed starts/cancellation, discovery failure escalation, successful setup, and truthful model reporting.

Both targets compiled successfully in [GitHub Actions run 38201820946](https://github.com/canyoncountryadventure/firmware/actions/runs/38201820946), using source commit `659d1f4ca0d9e6f4e83c4e3733ba799a062a28b6`. Target-specific UF2 verification and OTA ZIP integrity checks passed. See the [front-page README](README.md) for downloads, installation, commands, and the V4/V5 comparison. Hardware validation remains necessary: battery startup, logger absent/returning, disconnect during service discovery and reading, command replies during recovery, saved settings across watchdog reset, and a 24-hour run with hourly logger records.
