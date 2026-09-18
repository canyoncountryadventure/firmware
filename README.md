# Field Self-Recovery v2 — Canonical Source Baseline

This branch is the **shared hardened nRF52840 reliability baseline** for the Meshtastic environmental firmware family. Product branches such as RAK HOBO, soil moisture, water distance, Seeed HOBO, trail sensors, and Remote Drone Flashing retain their own sensor code and overlay this common recovery layer.

For normal use and firmware downloads, use the repository default branch:

**[Field-Recovery v2 firmware catalog](https://github.com/canyoncountryadventure/firmware/tree/field-self-recovery)**

For the full engineering record:

**[Final audit, architecture, and build specification](https://github.com/canyoncountryadventure/firmware/blob/field-self-recovery/docs/FIELD_RECOVERY_V2_AUDIT_AND_BUILD_SPEC.md)**

## What this baseline adds

- guarded SX1262 AGC/calibration maintenance with the 50 ms image-calibration settle;
- no invasive maintenance during active RX, active TX, queued TX, or pending IRQ state;
- missed RX_DONE and TX_DONE polling;
- recoverable RX/radio state instead of assert-and-die behavior;
- full SX1262 reinitialization and external radio-rail power cycling when supported;
- a second nRF52840 watchdog channel for field-health, independent of the main-loop watchdog;
- field watchdog operation during sleep/halt for these always-on environmental builds;
- flash quiesce before intentional nRF52 reset;
- accurate reset-reason reporting;
- larger main-loop, BLE-task, and callback-task stacks;
- a 12-hour preventive reboot during burn-in as an independent final fallback;
- pinned nRF52 framework and build-action revisions.

## Canonical builds

The workflow on this branch produces both reference targets:

| Target | Stable download |
|---|---|
| RAK4631 UF2 | [Field-Self-Recovery-v2-RAK4631.uf2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Field-Self-Recovery-v2-RAK4631.uf2) |
| RAK4631 BLE DFU | [Field-Self-Recovery-v2-RAK4631-OTA.zip](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Field-Self-Recovery-v2-RAK4631-OTA.zip) |
| Seeed XIAO nRF52840 UF2 | [Field-Self-Recovery-v2-Seeed.uf2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Field-Self-Recovery-v2-Seeed.uf2) |
| Seeed XIAO nRF52840 BLE DFU | [Field-Self-Recovery-v2-Seeed-OTA.zip](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Field-Self-Recovery-v2-Seeed-OTA.zip) |

## Recovery model

```text
normal operation
  -> missed IRQ recovery
  -> guarded radio maintenance
  -> RX retry
  -> SX1262 full re-init
  -> SX1262 power-cycle + re-init
  -> field-health watchdog trip
  -> nRF52840 hardware reset

independent burn-in fallback:
12-hour flash-safe whole-node reboot
```

## Product branches

Do **not** replace product branches with this tree. Each product branch is based on its sensor-specific v1 source, then receives this shared reliability layer. That preserves HOBO, soil-moisture, water-distance, trail, drone, and gateway behavior while keeping the field-recovery core consistent.
