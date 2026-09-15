# Water Distance Field Firmware

This branch is water-level only. There is no TRAIL mode and no MODE command is required. The node always runs the water-distance workflow.

## Defaults

- Sensor: SEN0313 / A01NYUB
- Reporting interval: 1 hour
- Calibration: unset
- Telemetry: Meshtastic PRIVATE_APP broadcast on channel 0 using the existing 24-byte `DS` water packet format
- Persistent water settings use two alternating CRC32-checked config slots for power-loss resilience
- A valid legacy distance calibration is migrated once and locked

## Field install

1. Mount the sensor permanently.
2. Send `RAW` and move/check the target to confirm fresh readings.
3. Send `VERIFY` for a 5-reading median and spread.
4. If needed, set the sensor with `SENSOR A01NYUB`, `SENSOR A02YYUW`, `SENSOR SEN0590`, or `SENSOR AUTO`.
5. Set interval, e.g. `INTERVAL 1H`.
6. Measure actual stage independently and send `CAL STAGE 1.42FT` (use the real value). Calibration automatically saves and locks.
7. Send `STATUS`. `Ready:YES` requires a fresh valid sensor reading plus valid locked calibration.
8. Reboot or power-cycle and send `STATUS` again to verify persistence.

## DM commands

Commands are case-insensitive. Bare commands, `DIST ...`, and `WATER ...` are accepted.

- `HELP`
- `STATUS`, `CHECK`, `INSTALL`
- `SENSOR`
- `SENSOR A01NYUB|A02YYUW|SEN0590|AUTO`
- `RAW`
- `READ`
- `VERIFY`
- `INTERVAL 15MIN|1H|6H` (5 seconds through 24 hours supported)
- `CAL STAGE <value><unit>` where units are `MM`, `CM`, `M`, `IN`, or `FT`
- `CAL STATUS`
- `CAL LOCK`
- `CAL UNLOCK` then `CAL UNLOCK CONFIRM`
- `CAL RESET` then `CAL RESET CONFIRM`
- `RESET WATER` then `RESET WATER CONFIRM` (resets only water subsystem; Meshtastic identity/channels/keys remain untouched)
- `TELEMETRY NOW`
- Recovery/health: `PING`, `VERSION`, `UPTIME`, `POWER`, `WATCHDOG`, `REBOOT`, `RECOVER`

Changing sensor type is blocked while calibration is locked. If calibration is unlocked and the sensor type is changed, the old calibration is cleared.

## A01NYUB blue LED / rapid ranging

The SEN0313/A01NYUB is an automatic continuous-output sensor whenever it has power. Its RX pin only chooses processed vs real-time output; it does not provide a sleep or interval command. Therefore firmware cannot make the sensor itself range only once per hour while VCC is continuously connected to 3.3 V/5 V.

To reduce the sensor LED/ranging to the measurement window, the sensor must be power-gated with an external high-side load switch or MOSFET controlled by the MCU. Do not power the sensor directly from an nRF52840 GPIO. A future power-gated build can turn the sensor on before a sample batch, wait for stabilization, read/verify, then turn it back off for the rest of the interval.
