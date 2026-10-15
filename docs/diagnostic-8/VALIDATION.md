# Diagnostic 8 validation

<!-- CURRENT_CI_START -->
## Current V5 release verification

Source `35755a47d455e8fb9aa044441d1c40eb9378269c`; [successful workflow](https://github.com/canyoncountryadventure/firmware/actions/runs/38663581582). The lifecycle fault tests, watchdog-reset command regressions and all 20 preservation checks passed. Both RAK4631 field environments and both autonomous Scout images compiled successfully. UF2 chip/address/source checks, application-only ZIP integrity/DAT CRC/UF2 equivalence, exact embedded Scout application and DAT, and both LZ4 decoders passed. Publication rechecked the field packages and matched Scout manifest hashes before creating the front-page links. Download checksums and source are in `downloads/RAK-HOBO-v5-Audit-Repair`.

No physical RAK/HOBO or airborne transfer was tested. Earlier local and unavailable native-suite results below remain distinct from this completed CI build.
<!-- CURRENT_CI_END -->


## Completed before build

- Compiled fault injection of actual firmware lifecycle functions: PASS. Covers successful callback setup, bounded failure of service/characteristic/notifications, disconnect during setup, stale handles, cancel success/INVALID_STATE/errors, late connection, scanner start/resume failures, address-type forwarding, non-connectable and locked-target filtering, sparse phone handles, truthful model history and clock rollover. The real main-loop setup gate is exercised during discovery.
- AddressSanitizer and UndefinedBehaviorSanitizer: PASS with `ASAN_OPTIONS=detect_leaks=0`. LeakSanitizer cannot inspect the host's `/proc` under this execution environment; leak detection is not claimed.
- Preservation checks: PASS for 20 unchanged baseline protocol/storage/build/DFU sections, both UUIDs and removal of persistent calibration mutation during distance reads. These are source-preservation checks, not physical flash-retention tests.
- `./mcp-server/run-tests.sh` with PlatformIO on PATH: **162 passed**, unit tier only. Pre-flight detected no supported hardware. Original `userPrefs.jsonc` restored and verified byte-for-byte after fixture execution, before firmware build.
- `git diff --check`: PASS.
- Local C++ formatting uses repository `.trunk/configs/.clang-format`; Python uses Black. Trunk and the local commit hook were attempted but automatic approval review blocked their outbound Sentry telemetry. No complete Trunk lint result is claimed. PlatformIO incidental telemetry was also blocked; its official `PLATFORMIO_SETTING_ENABLE_TELEMETRY=false` setting was verified in installed source to prevent queuing/sending before further builds.

## Build and package validation

Both RAK4631 sensor environments compiled successfully at software commit `7b57cd06a2e210f238af6f058c4616cef1e69397` (version `2.7.26.7b57cd0`). Both used 103,924/248,832 bytes of application RAM (41.8%) and 747,052/815,104 bytes of flash (91.7%). UF2 verification passed: nRF52840 family, correct source SHA and model strings, 2,919 blocks, application range `0x26000..0xDC700`, below filesystem `0xF4000`. Both OTA ZIPs passed integrity, application-only contents, matching UF2 payload (apart from documented zero UF2 block padding), device type `0x0052`, S140 requirement `0x00B6` and DAT CRC16 validation. Build log failures caught an initial LockGuard API mismatch; it was corrected and the host mock updated before these successful builds.

`pio test -e native` was attempted with telemetry disabled: all 15 discovered suites errored during build in this host environment, with missing `pkg-config` and `yaml-cpp/yaml.h`. These are **not passing tests** and are not evidence of a firmware regression. The targeted lifecycle tests and 162 Python host tests above ran successfully. No native-suite result is omitted or counted as a pass.

Matched autonomous Scout builds and CI package publication completed; see the current V5 release verification above. The final download source commit/checksums will be recorded in the download folder. Documentation/tool-only changes may produce a later embedded build SHA than the local software checkpoint above; only files verified together at the same target SHA may be paired.

## Hardware acceptance still required

Use one bench RAK4631 first, with matching sensor wiring and a known logger. Capture before/after `VERSION`, `LOGGER`, `WATER STATUS`, `WATER CAL STATUS`, channels, node identity and reset reason. Verify all three HOBO models, manual locks, unlock/autoselect, logger interval and write pointer, one transmission per new record, both ultrasonic sensors, power switching, correct stage/discharge, and retained calibration/configuration across app-only update and rollback.

For Swell Day, record advertising bytes/address type and the actual SoftDevice discovery submission result plus GATT responses at the BLE event task. `rsp=0` is insufficient to distinguish immediate rejection from a missing response. For Cottonwood, capture GAP connected/timeout/disconnected events independently of discovery. Repeat V3 versus repair on the same radio/logger, power source, distance and phone-app occupancy.

Soak for at least 48 hours under normal reporting plus repeated logger power loss, out-of-range, phone connection, USB serial API activity, sensor warm-up and sensor failure. Verify central reconnection without BLE-triggered hardware resets, continued mesh response during logger failure, no unexpected calibration writes, and correct model/pointer after recovery. Existing core/radio watchdog and 12-hour scheduled reboot must be distinguished from HOBO faults in logs.

Scout/drone bootloader entry, image transfer, post-update mesh callback and device-specific bootloader compatibility remain unverified physically. A structurally valid DFU package alone does not certify these.
