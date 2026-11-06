# RAK-HOBO-Safe-v5 — Field BLE Recovery V5.1 (2026-11-05)

**Target:** RAK4631 WisBlock (`rak4631`)  
**Meshtastic baseline:** 2.7.26; derived non-destructively from RAK-HOBO-Safe-v4.  
**HOBO models:** MX2001, MX2201, MX2203.

This experimental recovery update addresses unattended HOBO disconnections that persist while the LoRa radio still responds. It does not alter LoRa routing, the gateway, logger sampling interval, or the station's saved configuration.


## V5.1 BLE and telemetry repair (November 5, 2026)

This update keeps Meshtastic 2.7.26, logger sampling, the locked HOBO MAC, Nordic DFU, and saved mesh configuration intact.

- **Healthy HOBO:** leave the BLE link and STATUS polling alone; normal 30-second STATUS checks continue.
- **Disconnected:** immediately use a bounded two-minute, active 50% acquisition scan at boot, disconnect, and recovery; normal operation returns to the original passive 10% scan. A further two-minute burst occurs every five minutes while the logger remains missing.
- **Handshake timing:** a failed wake/handshake cooldown is 30 seconds, not five minutes.
- **15 minutes without successful HOBO STATUS:** rebuild the BLE central link and scanner state. A BLE advertisement, normal battery packet, or scanner flag does not count as recovery.
- **Still stale five minutes later:** one watchdog-origin whole-node reset recreates the nRF BLE/radio state without erasing configuration. A retained guard prevents reboot loops until a real STATUS reply returns.
- **Automatic telemetry:** MX2001 temperature and stage now use standard Meshtastic `TELEMETRY_APP` environment metrics, rather than the custom `PRIVATE_APP` MX payload. Battery and HOBO data therefore use the same normal transport family.
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

The dated V5.1 downloads appear only after GitHub Actions compilation and publication succeed. Verify the `COMMIT` and SHA-256 in the corresponding `*-BUILD.txt` before flashing.

**Field validation still required:** start with Mill Creek, verify that BLE STATUS and automatic telemetry resume after a disconnect, then assess battery consumption and the 12-hour reset through an entire day. Keep already healthy Pack Creek and Fishlake on their existing firmware.

## Key audit finding

V4 can report `scanner=SCANNING` for an arbitrarily long time without recognizing the locked HOBO, because the previous recovery watchdog is mainly activated by attempted connections or GATT STATUS/READ failures. The previously documented 12-hour whole-radio restart alone did not reliably restore HOBO operation.

## V5 changes

- Single scanner owner: the universal HOBO state machine (no competing scan supervisor).
- Normal passive scan remains 10% duty (320/32 units). A missing logger receives a two-minute active 50% burst (160/80 units) immediately and then at bounded five-minute intervals.
- Scanner refresh does not interrupt a connected link or in-progress BLE connection.
- Preserve persistent logger MAC locking and reconnect/retry behavior; no factory erasure.
- `HOBO RESTART` queues a BLE-only restart on the telemetry worker rather than resetting Meshtastic, and does not count as a failed recovery cycle.
- `BLE DIAG` or `HOBO STATUS` returns two compact LoRa messages with advertisement totals, locked-address matches, HOBO candidates, connection attempts/ready counts, scanner refresh counts, last STATUS age, and most recent BLE failure.
- `AUTO STATUS` returns logger-pointer tracking state and automatic transmit counts (boot-local). Manual `READ` remains independent of automatic record-pointer tracking.
- Existing V4 WDT diagnostics, 30-second STATUS poll interval, radio recovery, and 12-hour burn-in reboot are retained.

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

The separate `build_hobo_v5.yml` GitHub Actions workflow builds `rak4631` and publishes after successful compilation:

- [Nordic DFU OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-HOBO-Safe-v5-2026-11-05-OTA.zip)
- [USB UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-HOBO-Safe-v5-2026-11-05.uf2)
- [SHA256 and source commit](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-HOBO-Safe-v5-2026-11-05-BUILD.txt)

**The links are valid only after GitHub Actions successfully publishes the artifacts.** Do not distribute or flash an unverified file. This is the normal field-node firmware, not the autonomous drone Scout image; no matching V5 drone Scout artifact has been produced.

Do not unzip the OTA ZIP before selecting it in Nordic nRF Connect. Test on one disconnected station first; validate LOGGER matching, BLE reconnection, sensor readings, automatic telemetry across a real logger record boundary, and a complete sleep/wake or day/night cycle before wider deployment.

## Limits and recovery policy

V5.1 does **not** blindly reboot the whole node every two hours. It first rebuilds the BLE central after 15 minutes without a valid HOBO status; if status is still absent five minutes later, it permits one guarded watchdog reset. A verified HOBO status clears that guard. The existing 12-hour staggered reboot remains as a separate last-resort recovery layer.

The reason for a missing advertisement can still be range, physical placement, logger battery, logger advertisement state, or firmware. This build specifically addresses the central/GATT lifecycle wedge that can exist even while LoRa and battery telemetry continue working.

Counters are boot-local, and `status_seen=0` means no successful STATUS since boot. `target` advertisement counts require that the saved MAC matches the advertisement address, not merely the scanner being active. Only a field trial can establish reliable reconnect performance.
