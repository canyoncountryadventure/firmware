# Seeed-HOBO-Safe-v5 — Field BLE Recovery V5.1 (2026-11-04)

**Target:** Seeed XIAO nRF52840 + Wio-SX1262 (`seeed_xiao_nrf52840_kit`)  
**Meshtastic baseline:** 2.7.26; derived non-destructively from Seeed-HOBO-Safe-v4.  
**HOBO models:** MX2001, MX2201, MX2203.

This experimental recovery update addresses unattended HOBO disconnections that persist while the LoRa radio still responds. It does not alter LoRa routing, the gateway, logger sampling interval, or the station's saved configuration.


## V5 maintenance hardening (November 3, 2026)

The existing V5 source was updated in place, with **no change to Meshtastic 2.7.26**, logger sampling, the locked HOBO MAC, or BLE Nordic DFU support.

- **Healthy HOBO:** leave the BLE link and STATUS polling alone; normal 30-second STATUS checks continue.
- **Disconnected:** use an active 50%-duty BLE acquisition burst for two minutes at boot, after a disconnect/recovery, and every five missing minutes. Outside those bounded bursts, retain the original passive 10% scan.
- **15 minutes without successful HOBO STATUS:** perform an automatic BLE-only central-link / scanner-state reset. Clear stale rejected-candidate state. Do not disable the SoftDevice or reboot LoRa.
- **Still no STATUS five minutes later:** perform one watchdog-origin whole-node reset that rebuilds the nRF52 BLE/SoftDevice and radio stack while retaining settings. A retained guard permits only one such reset until a valid STATUS returns.
- **Persistently unhealthy:** repeat BLE-only recovery at the saved interval (default **120 minutes**), not every 15 minutes. A successful STATUS reply rearms the initial 15-minute health timeout and clears the one-reset guard.
- **Busy reads:** defer a scheduled BLE reset for up to 10 seconds to allow in-flight HOBO reads to complete; a stuck operation cannot block recovery forever.
- **Full node:** retain the existing 12-hour preventive reboot in Meshtastic's main loop, with up to ten minutes of deterministic per-node staggering and a 15-second send/read grace period. The reboot is not a complete electrical power cut to the external SX1262.
- **Radio fault:** retain existing guarded SX1262 receive/chip-state recovery and independent NRF hardware watchdog. Do not reset a healthy LoRa transceiver periodically.
- **Reboot diagnostics:** planned 12-hour and DM-initiated full reboots write an event in the existing retained nRF52 field diagnostic record, accessible via local serial `DIAG`, in addition to the hardware reset reason.

### Remote direct-message commands

```text
VERSION
BLE DIAG
BLE RESET STATUS
BLE RESET AUTO 120
BLE RESET AUTO OFF
HOBO RESTART
AUTO STATUS
WATCHDOG
STATS
REBOOT
```

`BLE RESET AUTO 120` sets and persists the repeat interval in minutes (valid 15–720). `BLE RESET AUTO OFF` disables scheduled deeper BLE-only resets, **not** the five-minute scanner refresh, the normal error recovery, watchdog, or 12-hour full reboot. The setting is stored independently from the saved HOBO MAC lock. `BLE RESET STATUS` reports the configured interval and reset counters.

The original V5 downloads are **replaced in place** only after new Seeed and RAK GitHub Actions compilation and publication succeed. Verify the `COMMIT` and SHA-256 in the corresponding `*-BUILD.txt` before flashing; older downloads at the same filename are not the current hardened build.

**Field validation still required:** start with Mill Creek, verify that BLE STATUS and automatic telemetry resume after a disconnect, then assess battery consumption and the 12-hour reset through an entire day. Keep already healthy Pack Creek and Fishlake on their existing firmware.

## Key audit finding

V4 can report `scanner=SCANNING` for an arbitrarily long time without recognizing the locked HOBO, because the previous recovery watchdog is mainly activated by attempted connections or GATT STATUS/READ failures. The previously documented 12-hour whole-radio restart alone did not reliably restore HOBO operation.

## V5 changes

- Single scanner owner: the universal HOBO state machine (no competing scan supervisor).
- Normal passive scan remains 10% duty (320/32 units). At boot, after disconnect/recovery, and every five missing minutes, use active 50% duty (160/80 units) for two minutes, then return to passive 10%.
- Scanner refresh does not interrupt a connected link or in-progress BLE connection.
- Preserve persistent logger MAC locking and reconnect/retry behavior; no factory erasure.
- `HOBO RESTART` queues a BLE-only restart on the telemetry worker rather than resetting Meshtastic, and does not count as a failed recovery cycle.
- `BLE DIAG` or `HOBO STATUS` returns two compact LoRa messages with advertisement totals, locked-address matches, HOBO candidates, connection attempts/ready counts, scanner refresh counts, last STATUS age, and most recent BLE failure.
- `AUTO STATUS` returns logger-pointer tracking state and automatic transmit counts (boot-local). Manual `READ` remains independent of automatic record-pointer tracking.
- Existing V4 WDT diagnostics, STATUS poll interval, guarded radio recovery, and 12-hour burn-in reboot are retained. Ordinary BLE faults never cause a reboot loop.

### Standard telemetry transport

MX2001 automatic reports now use normal Meshtastic `TELEMETRY_APP` environment metrics: temperature in °C and calibrated stage in `distance` as millimetres (`stage_ft × 304.8`). Device/battery and environmental metrics are separate protobuf messages, but both use the same normal telemetry port. The companion Heltec gateway recognizes the paired temperature-plus-distance MX2001 form only from the known Pack Creek, Mill Creek, and Matheson MX2001 node IDs and uploads it as `mx2001` data. Manual `READ` still includes logger MAC and BLE RSSI.

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

- [Nordic DFU OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Seeed-HOBO-Safe-v5-2026-11-04-OTA.zip)
- [USB UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Seeed-HOBO-Safe-v5-2026-11-04.uf2)
- [SHA256 and source commit](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Seeed-HOBO-Safe-v5-2026-11-04-BUILD.txt)

**The links are valid only after GitHub Actions successfully publishes the artifacts.** Do not distribute or flash an unverified file. This is the normal field-node firmware, not the autonomous drone Scout image; no matching V5 drone Scout artifact has been produced.

Do not unzip the OTA ZIP before selecting it in Nordic nRF Connect. Test on one disconnected station first; validate LOGGER matching, BLE reconnection, sensor readings, automatic telemetry across a real logger record boundary, and a complete sleep/wake or day/night cycle before wider deployment.

## Limits and recovery policy

V5.1 does **not** assume a fixed 2-hour full-node reboot will solve a missing HOBO advertisement. It uses active scanner acquisition, a BLE-only recovery, then one status-proven guarded whole-node reset. Strong Mill Creek BLE evidence means range is not the universal explanation; a scanner/GATT/SoftDevice wedge can exist while LoRa battery telemetry still works.

Counters are boot-local, and `status_seen=0` means no successful STATUS since boot. `target` advertisement counts require that the saved MAC matches the advertisement address, not merely the scanner being active. Only a field trial can establish reliable reconnect performance.
