# October 17, 2026 firmware upgrade acceptance criteria

This document defines requirements, not implemented features. Do not mark complete without source audit, CI and hardware evidence.

## Shared DM design
- Preserve existing commands and add short aliases only when no parser conflicts exist.
- DIAG must return bounded DM-readable reset reason, boot count, uptime, radio/BLE state, last sensor result, battery voltage/percentage, and last error. Serial-only output is insufficient.
- HUB STATUS / HUB SET / HUB CLEAR: when unset, preserve ordinary broadcast behavior. Confirm persistence and routing in code.
- SAMPLES STATUS / SAMPLES SET n: bounded persistent count; maintain correct median and timeout semantics; report effective value.
- BATTERY: measured voltage, derived percentage, charging status only if hardware exposes it, and relevant low-voltage counters.
- LAST RESORT: explicitly unacknowledged recovery request; no confirmation dependency. Require precise command syntax, replay protection and safeguards against accidental execution.
- Keep WATCHDOG, SOFT REBOOT, REBOOT, RECOVER and CRASHLOG semantics explicit. Verify automatic reset cadence from source before documentation.
- Avoid blocking sensor warm-ups or BLE scheduler starvation.

## RAK V6
- Preserve proven V5 8-second nonblocking warm-up, passive 10% BLE scanning, verified MX2001 handshake and saved calibration.
- Audit precision end-to-end: MX2001 value decoding, packet representation, gateway parsing and Neon storage. Display two decimal places only when the measurement genuinely supports them.
- Retain SEN0313 and MB7388 variants and matched drone Scout packages.

## Heltec V5
- Preserve gateway ingestion and existing packet schemas, Wi-Fi OTA, telemetry and direct HOBO BLE.
- Check radio watchdog and 12-hour preventive reboot; preserve reset breadcrumbs.
- Verify gateway accepts any new RAK DM/telemetry formats without breaking legacy senders.

## Release gate
- New dated filenames (actual build date) for every UF2, OTA ZIP, and Scout image.
- Build both RAK sensor variants and Heltec target; verify binary identities, payload checksums and matching drone payloads.
- Publish exact download links only after verifying file availability.
- README must document DM command grammar, defaults, persistence, resets, upgrade, rollback, known limitations and validation results.
- Do not overwrite previous release binaries.
