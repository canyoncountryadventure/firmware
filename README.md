# Meshtastic Field Firmware Catalog — Current HOBO V5.1

This is the download catalog for the current HOBO BLE-recovery release. It separates the ordinary field-node install files from the separate RAK4631 drone Scout files so they cannot be confused.

## Current target and Scout pairs

| Field target | Field USB UF2 | Field Nordic OTA ZIP | Target manifest | Matching drone Scout UF2 | Scout manifest |
|---|---|---|---|---|---|
| **RAK HOBO Safe V5.1** — RAK4631 / RAK19007 | [USB UF2](downloads/RAK-HOBO-Safe-v5-2026-11-05.uf2) | [Nordic OTA ZIP](downloads/RAK-HOBO-Safe-v5-2026-11-05-OTA.zip) | [BUILD.txt](downloads/RAK-HOBO-Safe-v5-2026-11-05-BUILD.txt) | [Drone Scout UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Remote-Drone-Flashing-v2/downloads/Drone-RAK-HOBO-Safe-v5-2026-11-05.uf2) | [Scout manifest](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Remote-Drone-Flashing-v2/downloads/Drone-RAK-HOBO-Safe-v5-2026-11-05.uf2.txt) |
| **Chew can HOBO Safe V5.1** — Seeed XIAO nRF52840 + Wio-SX1262 | [USB UF2](downloads/Seeed-HOBO-Safe-v5-2026-11-04.uf2) | [Nordic OTA ZIP](downloads/Seeed-HOBO-Safe-v5-2026-11-04-OTA.zip) | [BUILD.txt](downloads/Seeed-HOBO-Safe-v5-2026-11-04-BUILD.txt) | [Drone Scout UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Remote-Drone-Flashing-v2/downloads/Drone-Seeed-HOBO-Safe-v5-2026-11-05.uf2) | [Scout manifest](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Remote-Drone-Flashing-v2/downloads/Drone-Seeed-HOBO-Safe-v5-2026-11-05.uf2.txt) |

The Scout manifests and target BUILD files must agree on the target source commit and target OTA SHA-256. A board label alone is not enough; RAK and Chew can payloads are different applications.

## Which file goes where

| File | Destination | Use |
|---|---|---|
| Target `*.uf2` | The field station | USB bootloader installation or recovery |
| Target `*-OTA.zip` | The field station | Nordic nRF Connect BLE update; choose the unopened ZIP |
| Dated `Drone-*.uf2` | The separate drone RAK4631 Scout | Autonomous nearby BLE update of its one matching target |
| `*-BUILD.txt` / `Drone-*.uf2.txt` | Bench checklist | Exact target/Scout pairing and checksums |

Never flash a `Drone-*.uf2` to a field station. The drone Scout is a dedicated BLE flasher and does not run the normal field-node application.

## Drone update sequence

1. Flash or verify the normal V5.1 target firmware on the field node first.
2. Flash the matching dated Scout UF2 to the separate RAK4631 that rides on the drone.
3. Put the Scout near the field target and direct-message the target `DFU`.
4. The target reboots into Nordic BLE DFU; the Scout transfers its exact embedded OTA application.
5. Wait for the Scout result and the target’s mesh callback. Keep the matching target USB UF2 ready for physical recovery.

The Scout protocol is application-only: it does not intentionally erase the target’s mesh configuration, keys, or locked HOBO assignment. An interrupted BLE transfer can still require USB recovery.

## What changed in V5.1

- A missing HOBO gets a two-minute active BLE acquisition scan at boot, disconnection, and bounded five-minute retry windows.
- The failed handshake cooldown is 30 seconds rather than five minutes.
- A valid HOBO `STATUS` reply—not a scanner flag, advertisement, LoRa response, or battery telemetry—is the health proof.
- Fifteen minutes without status rebuilds the BLE central; a further five minutes permits one retained-guard watchdog reset instead of a reboot loop.
- HOBO automatic readings use normal Meshtastic `TELEMETRY_APP` environmental metrics, matching the ordinary telemetry transport family used by battery data.

## Source and verification

- [RAK HOBO Safe V5.1 source](https://github.com/canyoncountryadventure/firmware/tree/RAK-HOBO-Safe-v5)
- [Chew can HOBO Safe V5.1 source](https://github.com/canyoncountryadventure/firmware/tree/Seeed-HOBO-Safe-v5)
- [Drone Scout catalog and field procedure](https://github.com/canyoncountryadventure/firmware/tree/Remote-Drone-Flashing-v2)

The two dated Scout packages were CI-built from the pinned target OTA files and checked so their final UF2s contain the exact LZ4-compressed application and Nordic init packet. The Scout DFU protocol has been bench proven previously; run one accessible end-to-end V5.1 target/Scout test before trusting the new pair at an unreachable station.

## Legacy files

Older V2–V5 target files remain under [`downloads/`](downloads/) for rollback and forensic comparison. They are not the current release path. Use the dated V5.1 rows above unless you are deliberately reproducing an older deployment.
