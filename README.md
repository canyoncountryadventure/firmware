# HOBO Drone Flashers — Current V5.1

This branch holds the firmware for the separate **RAK4631 drone Scout**. A Scout carries one exact, compressed Nordic OTA application and transfers it to a nearby field node over BLE after that node receives a direct-message `DFU` command.

**Do not flash a `Drone-*.uf2` to a field station.** It belongs only on the separate RAK4631 carried by the drone. The target row and Scout row must match exactly.

## Current verified packages

| Field target | Field USB UF2 | Field Nordic OTA ZIP | Matching drone Scout UF2 | Exact-payload manifest |
|---|---|---|---|---|
| **RAK HOBO Safe V5.1** — RAK4631 | [USB UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-HOBO-Safe-v5-2026-11-05.uf2) | [OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-HOBO-Safe-v5-2026-11-05-OTA.zip) | [Drone-RAK-HOBO-Safe-v5-2026-11-05.uf2](downloads/Drone-RAK-HOBO-Safe-v5-2026-11-05.uf2) | [Scout manifest](downloads/Drone-RAK-HOBO-Safe-v5-2026-11-05.uf2.txt) |
| **Chew can HOBO Safe V5.1** — Seeed XIAO nRF52840 + Wio-SX1262 | [USB UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Seeed-HOBO-Safe-v5-2026-11-04.uf2) | [OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Seeed-HOBO-Safe-v5-2026-11-04-OTA.zip) | [Drone-Seeed-HOBO-Safe-v5-2026-11-05.uf2](downloads/Drone-Seeed-HOBO-Safe-v5-2026-11-05.uf2) | [Scout manifest](downloads/Drone-Seeed-HOBO-Safe-v5-2026-11-05.uf2.txt) |

The Scout manifests name the target branch, exact target source commit, pinned OTA SHA-256, source catalog commit, and the Scout’s own SHA-256. The target `*-BUILD.txt` and matching Scout manifest must name the same target commit before use.

## What V5.1 fixes

Both current target packages contain the HOBO BLE repair: active acquisition scans after a missing logger, a 30-second handshake cooldown rather than five minutes, status-proven BLE recovery, one guarded whole-node reset if recovery fails, and standard Meshtastic `TELEMETRY_APP` environmental packets for HOBO readings. The Scout embeds the exact normal OTA application; it does not alter the target’s settings, HOBO lock, or mesh configuration.

## Which file goes where

| File | Goes on | Purpose |
|---|---|---|
| Target `*.uf2` | The field RAK or Chew can | USB bootloader install or recovery |
| Target `*-OTA.zip` | The field RAK or Chew can | Phone BLE update through Nordic nRF Connect; select the unopened ZIP |
| `Drone-*.uf2` | The separate drone RAK4631 Scout | Autonomous nearby BLE updater for one matching target type |
| `Drone-*.uf2.txt` | Your bench checklist | Confirms the exact embedded target before flight |

The RAK Scout can flash either a RAK target or a Chew can target, but only when it is loaded with that target’s matching dated Scout UF2.

## Field sequence

1. Put the normal field firmware on the target first and keep its USB UF2 available for recovery.
2. Put the matching dated `Drone-*.uf2` on the separate RAK4631 Scout by double-pressing reset and copying the UF2 to its bootloader drive.
3. Place the Scout close to the target and power it before flight.
4. Direct-message the target `DFU`.
5. The target reboots into its Nordic BLE DFU bootloader; the Scout finds it and streams the embedded application.
6. Wait for the Scout success indication and the target’s post-update mesh callback before leaving the site.

The update is application-only. Keep the matching field USB UF2 with you: an interrupted BLE transfer may require physical USB recovery.

## Validation status

| Layer | Status |
|---|---|
| RAK4631 Scout implementation and Nordic Legacy DFU protocol | Previously bench proven |
| Current V5.1 target packages | Built and published from their dated target branches |
| Current V5.1 Scout package contents | CI-built; compressed payload, Nordic init packet, and nRF52840 UF2 layout verified byte-for-byte |
| V5.1 end-to-end field pair | Bench-check once on an accessible node before using it on an unreachable station |

## Legacy files

Older V2–V5 Scout images remain in [`downloads/`](downloads/) for rollback and forensic comparison. They are not current release choices. Do not select an older file merely because its name matches the hardware; use the dated V5.1 row above.

## Build and verification

[`build_hobo_safe_v5_drone_scouts.yml`](.github/workflows/build_hobo_safe_v5_drone_scouts.yml) downloads each pinned, published target OTA by immutable catalog commit, checks its SHA-256, compresses the application with LZ4, embeds the paired Nordic init packet, builds the Scout, then verifies the final UF2 contains those exact bytes before publishing it.

The Scout implementation is [here](src/experimental/rak4631_drone_flasher.cpp). The payload generator and verifier are [here](tools/build_drone_embedded_firmware.py) and [here](tools/verify_drone_hobo_safe_v5.py).
