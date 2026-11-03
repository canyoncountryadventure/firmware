# Seeed-HOBO-Safe-v5 — Field BLE Recovery V5 (2026-11-03)

**Target:** Seeed XIAO nRF52840 + Wio-SX1262 (`seeed_xiao_nrf52840_kit`)  
**Meshtastic baseline:** 2.7.26; derived non-destructively from Seeed-HOBO-Safe-v4.  
**HOBO models:** MX2001, MX2201, MX2203.

This experimental recovery update addresses unattended HOBO disconnections that persist while the LoRa radio still responds. It does not alter LoRa routing, the gateway, logger sampling interval, or the station's saved configuration.

## Key audit finding

V4 can report `scanner=SCANNING` for an arbitrarily long time without recognizing the locked HOBO, because the previous recovery watchdog is mainly activated by attempted connections or GATT STATUS/READ failures. The previously documented 12-hour whole-radio restart alone did not reliably restore HOBO operation.

## V5 changes

- Single scanner owner: the universal HOBO state machine (no competing scan supervisor).
- Normal passive scan remains 10% duty (320/32 units). After five disconnected minutes, restart at 50% duty (160/80 units) for 30 seconds, then return to 10%; repeat in bounded five-minute intervals if still disconnected.
- Scanner refresh does not interrupt a connected link or in-progress BLE connection.
- Preserve persistent logger MAC locking and reconnect/retry behavior; no factory erasure.
- `HOBO RESTART` queues a BLE-only restart on the telemetry worker rather than resetting Meshtastic, and does not count as a failed recovery cycle.
- `BLE DIAG` or `HOBO STATUS` returns two compact LoRa messages with advertisement totals, locked-address matches, HOBO candidates, connection attempts/ready counts, scanner refresh counts, last STATUS age, and most recent BLE failure.
- `AUTO STATUS` returns logger-pointer tracking state and automatic transmit counts (boot-local). Manual `READ` remains independent of automatic record-pointer tracking.
- Existing V4 WDT diagnostics, STATUS poll interval, watchdog escalation on exhausted BLE commands, radio recovery, and 12-hour burn-in reboot are retained.

### DM verification

Send these individually to the station:

```text
VERSION
LOGGER
BLE DIAG
AUTO STATUS
READ
HOBO RESTART
BLE DIAG
LOGGER
```

`HOBO RESTART` is **non-destructive**. It does not unlock a logger, erase data, or reboot the LoRa radio. `DIAG` in the old V4 module remains a local-serial diagnostic; use **BLE DIAG** for the new remote statistics.

## Build and flash

The separate `build_hobo_v5.yml` GitHub Actions workflow builds `seeed_xiao_nrf52840_kit` and publishes after successful compilation:

- [Nordic DFU OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Seeed-HOBO-Safe-v5-2026-11-03-OTA.zip)
- [USB UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Seeed-HOBO-Safe-v5-2026-11-03.uf2)
- [SHA256 and source commit](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Seeed-HOBO-Safe-v5-2026-11-03-BUILD.txt)

**The links are valid only after GitHub Actions successfully publishes the artifacts.** Do not distribute or flash an unverified file. This is the normal field-node firmware, not the autonomous drone Scout image; no matching V5 drone Scout artifact has been produced.

Do not unzip the OTA ZIP before selecting it in Nordic nRF Connect. Test on one disconnected station first; validate LOGGER matching, BLE reconnection, sensor readings, automatic telemetry across a real logger record boundary, and a complete sleep/wake or day/night cycle before wider deployment.

## Limits and recovery policy

V5 does **not** assume a 2-hour full-node reboot will solve a missing HOBO advertisement. It emphasizes scanner-level recovery with bounded high-duty bursts and preserves the existing radio watchdog fallback. The reason for missing advertisements may be range, physical placement, logger battery, logger advertisement state, or firmware.

Counters are boot-local, and `status_seen=0` means no successful STATUS since boot. `target` advertisement counts require that the saved MAC matches the advertisement address, not merely the scanner being active. Only a field trial can establish reliable reconnect performance.
