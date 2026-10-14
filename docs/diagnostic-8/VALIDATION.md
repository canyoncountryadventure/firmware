# Diagnostic 8 validation

## Completed before build

- Compiled fault injection of actual firmware lifecycle functions: PASS. Covers successful callback setup, bounded failure of service/characteristic/notifications, disconnect during setup, stale handles, cancel success/INVALID_STATE/errors, late connection, scanner start/resume failures, address-type forwarding, non-connectable and locked-target filtering, sparse phone handles, truthful model history and clock rollover. The real main-loop setup gate is exercised during discovery.
- AddressSanitizer and UndefinedBehaviorSanitizer: PASS with `ASAN_OPTIONS=detect_leaks=0`. LeakSanitizer cannot inspect the host's `/proc` under this execution environment; leak detection is not claimed.
- Preservation checks: PASS for 20 unchanged baseline protocol/storage/build/DFU sections, both UUIDs and removal of persistent calibration mutation during distance reads. These are source-preservation checks, not physical flash-retention tests.
- `./mcp-server/run-tests.sh` with PlatformIO on PATH: **162 passed**, unit tier only. Pre-flight detected no supported hardware. Original `userPrefs.jsonc` restored and verified byte-for-byte after fixture execution, before firmware build.
- `git diff --check`: PASS.
- Local C++ formatting uses repository `.trunk/configs/.clang-format`; Python uses Black. Trunk was attempted but automatic approval review blocked its outbound Sentry telemetry. No complete Trunk lint result is claimed.

## Build and package validation

Firmware builds and native C++ tests are in progress at this source checkpoint. Do not treat this checkpoint as a completed or hardware-validated firmware release. Final results and verified artifact links will replace this paragraph after completion.

## Hardware acceptance still required

Use one bench RAK4631 first, with matching sensor wiring and a known logger. Capture before/after `VERSION`, `LOGGER`, `WATER STATUS`, `WATER CAL STATUS`, channels, node identity and reset reason. Verify all three HOBO models, manual locks, unlock/autoselect, logger interval and write pointer, one transmission per new record, both ultrasonic sensors, power switching, correct stage/discharge, and retained calibration/configuration across app-only update and rollback.

For Swell Day, record advertising bytes/address type and the actual SoftDevice discovery submission result plus GATT responses at the BLE event task. `rsp=0` is insufficient to distinguish immediate rejection from a missing response. For Cottonwood, capture GAP connected/timeout/disconnected events independently of discovery. Repeat V3 versus repair on the same radio/logger, power source, distance and phone-app occupancy.

Soak for at least 48 hours under normal reporting plus repeated logger power loss, out-of-range, phone connection, USB serial API activity, sensor warm-up and sensor failure. Verify central reconnection without BLE-triggered hardware resets, continued mesh response during logger failure, no unexpected calibration writes, and correct model/pointer after recovery. Existing core/radio watchdog and 12-hour scheduled reboot must be distinguished from HOBO faults in logs.

Scout/drone bootloader entry, image transfer, post-update mesh callback and device-specific bootloader compatibility remain unverified physically. A structurally valid DFU package alone does not certify these.
