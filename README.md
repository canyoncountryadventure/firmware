# Field Self-Recovery V4

V4 is the hardened canonical HOBO field-recovery firmware for Nordic nRF52 radios. This branch supports only these two targets:

- RAK4631.
- Seeed XIAO nRF52840 + Wio-SX1262.

Heltec is not a V4 target in this branch and is not modified by this work.

## Verified V4 downloads

The target files below are published by the V4 build workflow to the durable `field-self-recovery/downloads` catalog.

### RAK4631

- [V4 field-radio UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Field-Self-Recovery-v4-RAK4631.uf2)
- [V4 field-radio OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Field-Self-Recovery-v4-RAK4631-OTA.zip)
- [V4 build manifest](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Field-Self-Recovery-v4-RAK4631-BUILD.txt)
- V4 compressed drone Scout UF2: **unavailable**. No V4 RAK4631 Scout artifact is currently published on `Remote-Drone-Flashing-v2`.

### Seeed XIAO nRF52840 + Wio-SX1262

- [V4 field-radio UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Field-Self-Recovery-v4-Seeed.uf2)
- [V4 field-radio OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Field-Self-Recovery-v4-Seeed-OTA.zip)
- [V4 build manifest](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Field-Self-Recovery-v4-Seeed-BUILD.txt)
- V4 compressed drone Scout UF2: **unavailable**. No V4 Seeed Scout artifact is currently published on `Remote-Drone-Flashing-v2`.

Do not substitute a V2 or V3 Scout image and describe it as V4. A Scout image must be built for the exact target image and verified before remote drone flashing.

The durable filenames above are updated by successful V4 builds. The build manifest records the source commit and SHA-256 hashes for the published target files.

## V4 recovery diagnostics

V4 adds retained nRF52 field diagnostics in a `.noinit` RAM record. The record tracks:

- boot count;
- reset reason seen at boot;
- last recorded uptime;
- failing subsystem and operation;
- an associated error value.

The record is initialized only when its magic value is absent, so failure context remains available across watchdog and software resets while the retained RAM contents survive. It is not a substitute for nonvolatile storage and should not be expected to survive loss of power.

At every boot, V4 logs the reset reason and the retained field-diagnostic record to the local serial log. Normal loop uptime updates do not erase the last subsystem/operation failure context.

Direct-message diagnostics are also available through the HOBO self-recovery module:

- `DIAG` or `CRASHLOG` prints the retained diagnostic record to the local serial log.
- `CLEAR DIAG` clears retained failure context while preserving the boot count.
- `STATS` and `HEALTH` expose current recovery/reset summary information over the mesh.

## Watchdog and bounded recovery behavior

V4 is designed to reset instead of hanging forever when a recoverable nRF52 subsystem stops responding.

- BLE passkey waiting is bounded to 30 seconds and yields while waiting.
- BLE disconnect waiting is bounded to 5 seconds. A timeout is recorded for field diagnostics instead of blocking indefinitely.
- LPCOMP readiness before SYSTEM OFF is bounded to 1 second. If LPCOMP never becomes ready, V4 records the failure and resets instead of spinning forever.
- Exhausted SX1262/radio recovery records radio-specific failure context, then deliberately stops feeding the field watchdog so the hardware watchdog can reset the node.
- Exhausted HOBO BLE/telemetry recovery records telemetry-specific failure context, then uses the same watchdog-reset path.
- The watchdog-trip path preserves the caller's subsystem-specific diagnostic context so the following boot can report the actual recovery cause.

These safeguards are recovery boundaries, not proof that a reset will repair a physical wiring, power, antenna, logger, or RF problem.

## HOBO operation retained in V4

The canonical V4 images retain the established HOBO behavior:

- HOBO STATUS polling every 30 seconds.
- Automatic mesh telemetry only after a confirmed new logger write-pointer advance.
- Suppression of implausibly rapid repeated pointer advances so noisy STATUS responses do not create a one-second transmission flood.
- Manual `READ`, logger locking, reconnect/recovery, status, health, and DFU commands.
- Remote target `DFU` support remains present in the RAK4631 and Seeed nRF52840 targets.

The 30-second STATUS poll is an observation interval between the radio and logger. It does not change the HOBO logger's configured recording interval and does not itself cause a mesh transmission every 30 seconds.

## Verification scope

GitHub Actions verifies compilation and packaging for both supported targets. Before unattended deployment, verify the intended logger MAC, `READ`, `LOGGER`, `STATUS`, `DIAG`, `VERSION`, and `DFU` behavior on the actual hardware.

A successful CI build does not by itself prove physical BLE range, HOBO sensor behavior, LoRa delivery, power-system stability, or drone DFU operation. V4 Scout artifacts are therefore left explicitly unavailable until matching Scout images exist and are separately verified.
