> **CURRENT BRANCH: RAK-Water-Distance-HOBO-v3 — HOBO V3.** Every firmware file linked in the **V3 downloads** table below has `-v3` in its filename. The unchanged `Remote-Drone-Flashing-v2` branch hosts the **V3** compressed Scout files; its branch name is not the firmware image version. The rest of this document describes this V3 configuration (including inherited V2 safeguards).

## V3 downloads — use the file for this exact station

| File | Purpose | Link |
|---|---|---|
| `RAK-Water-Distance-HOBO-v3.uf2` | **V3 field radio** — normal USB UF2, for the RAK4631 node | [Download V3 target UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-Water-Distance-HOBO-v3.uf2) |
| `RAK-Water-Distance-HOBO-v3-OTA.zip` | **V3 field radio** — BLE OTA update package (not a UF2) | [Download V3 target OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-Water-Distance-HOBO-v3-OTA.zip) |
| `Drone-RAK-Water-Distance-HOBO-v3.uf2` | **V3 drone Scout** — flash ONLY onto the *separate RAK4631 carried by the drone*, never onto the field node | [Download matching V3 drone Scout UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Remote-Drone-Flashing-v2/downloads/Drone-RAK-Water-Distance-HOBO-v3.uf2) |
| `RAK-Water-Distance-HOBO-v3-BUILD.txt` | Target build commit and checksums | [View V3 build manifest](https://github.com/canyoncountryadventure/firmware/blob/field-self-recovery/downloads/RAK-Water-Distance-HOBO-v3-BUILD.txt) |
| `Drone-RAK-Water-Distance-HOBO-v3.uf2.txt` | Scout's embedded target branch, target commit, and checksum | [View V3 Scout manifest](https://github.com/canyoncountryadventure/firmware/blob/Remote-Drone-Flashing-v2/downloads/Drone-RAK-Water-Distance-HOBO-v3.uf2.txt) |

**Before a remote update:** compare the target commit in the Scout manifest with the target build manifest. If these differ, use a fresh matching Scout image; do not assume different builds are paired. For USB updates, use the normal target UF2, **not** the `Drone-` UF2.

**V3 operating behavior:** `STATUS` checks the HOBO logger write pointer on a **30-second** healthy-link cadence. The HOBO's internal logging interval is unchanged; automatic mesh telemetry is sent only on a confirmed new record. A manual `READ` remains independent. V3 inherits non-destructive field recovery and the target's existing `DFU` command.

**Source:** [RAK-Water-Distance-HOBO-v3](https://github.com/canyoncountryadventure/firmware/tree/RAK-Water-Distance-HOBO-v3) · **V3 target build:** [GitHub Actions](https://github.com/canyoncountryadventure/firmware/actions/workflows/build_hobo_v3.yml?query=branch%3ARAK-Water-Distance-HOBO-v3) · **V3 matching Scout build:** [GitHub Actions](https://github.com/canyoncountryadventure/firmware/actions/workflows/build_hobo_v3_drone_catalog.yml).

---

> **HOBO V3 (retaining Field-Recovery v2 safeguards):** hardened SX1262 recovery, dual nRF52 watchdog channels, missed RX/TX IRQ polling, guarded AGC calibration, radio rail power-cycle recovery, flash-safe reboot, 12-hour burn-in reboot, and increased nRF52/BLE task stacks. Sensor behavior from RAK-Water-Distance-HOBO-v1 is retained.

# RAK Water Distance + HOBO v3

Production combined firmware for **RAK4631 + RAK19007** field nodes that must do both:

- ultrasonic water-level / stage monitoring, and
- BLE collection from supported HOBO MX loggers.

**Branch:** `RAK-Water-Distance-HOBO-v3`

This branch is built from the current **RAK Water Distance v1** firmware and then adds the proven HOBO reader/recovery stack. It replaces the older combined distance/HOBO builds.

## Scope

Water side:

- DFRobot SEN0313 / A01NYUB — default
- DFRobot SEN0311 / A02YYUW
- DFRobot SEN0590
- permanent water mode
- fresh-frame distance reads
- stage calibration with automatic lock
- configurable report interval
- redundant A/B persistent config with sequence, CRC32, exact-size checking, truncation before overwrite, and read-back verification
- water telemetry on Meshtastic channel 0

HOBO side:

- MX2001
- MX2201
- MX2203
- BLE scanning/collection using the existing universal HOBO reader
- HOBO diagnostics plus automatic BLE link recovery owned by the HOBO telemetry state machine

Trail-counter logic is intentionally excluded.

## Recovery ownership

The combined build uses **one node-recovery command/diagnostic module**: the HOBO self-recovery module. The standalone distance recovery supervisor is not instantiated. BLE scanner/link lifecycle remains owned by the HOBO telemetry state machine, which handles connection timeouts and repeated STATUS/NEWREAD failures. This avoids competing BLE owners and duplicate reboot command handlers while retaining the independent Field-Recovery v2 watchdog escalation path.

Water configuration persistence remains handled by the Water Distance v1 module.

## Water field setup

Use a second Meshtastic node to DM the field node:

```text
WATER STATUS
WATER RAW
WATER VERIFY
WATER INTERVAL 1H
WATER CAL STAGE 1.42FT
WATER CAL STATUS
WATER TELEMETRY NOW
```

Replace `1.42FT` with the independently measured stage at the site. The `WATER` prefix is **required** in this combined build. Bare `READ`, `STATUS`, and other unprefixed commands are reserved for the HOBO/recovery side so the two command systems cannot collide.

After calibration, completely remove power, reconnect it, then verify:

```text
WATER STATUS
WATER CAL STATUS
WATER READ
```

To discard a bench/test calibration before deployment:

```text
WATER CAL RESET CONFIRM
```

This clears only water calibration; sensor selection and reporting interval are preserved.

## RAK UART mapping

```text
UART1 RX -> logical pin 15
UART1 TX -> logical pin 16
```

For the streaming A01NYUB, sensor TX goes to the node UART RX.

## Water persistence

Water settings are stored in redundant A/B flash records. The inactive slot is rewound and truncated before each write, then the complete record is re-opened and verified before it becomes active. The previous valid slot remains the fallback until verification succeeds.

Normal battery loss, solar shutdown, reboot, watchdog reset, or ordinary non-destructive firmware updates do not intentionally erase water calibration, interval, Meshtastic identity, channels, keys, or NodeDB.

## A01NYUB power note

A01NYUB ranges continuously while powered, so its blue LED keeps blinking even when water telemetry is hourly. Actual sensor sleep requires hardware power gating with a load switch or high-side MOSFET.

## Useful DM commands

Send bare `HELP` for the complete six-message station command reference. Send `WATER HELP` for the complete four-message water-only reference. Bare `READ` is the **HOBO** read; water commands are deliberately prefixed with `WATER` so the two sensor systems cannot collide.

### Water side

| Command | What it does |
|---|---|
| `WATER` | Alias for `WATER HELP`. |
| `WATER HELP` | Lists every supported water command in four numbered messages. |
| `WATER STATUS` | Reports water sensor, interval, calibration, and readiness. |
| `WATER CHECK` | Runs the water readiness/status check. |
| `WATER INSTALL` | Runs the same installation/readiness status check. |
| `WATER SENSOR` | Shows configured and active distance-sensor drivers. |
| `WATER SENSOR AUTO` | Automatically tries supported distance-sensor drivers. |
| `WATER SENSOR SEN0590` | Selects the SEN0590 I2C driver. |
| `WATER SENSOR SEN0311` | Selects the SEN0311 UART driver. |
| `WATER SENSOR A02YYUW` | Alias for the SEN0311/A02YYUW UART driver. |
| `WATER SENSOR SEN0313` | Selects the SEN0313 UART driver. |
| `WATER SENSOR A01NYUB` | Alias for the SEN0313/A01NYUB UART driver. |
| `WATER READ` | Takes a fresh water-distance reading and reports stage if calibrated. |
| `WATER RAW` | Returns raw distance without stage conversion. |
| `WATER VERIFY` | Takes multiple fresh readings and reports median/range/spread. |
| `WATER INTERVAL 1H` | Saves the automatic water-report interval; duration units are accepted by the parser. |
| `WATER CAL STATUS` | Shows saved water calibration and lock state. |
| `WATER CAL STAGE 1.42FT` | Calibrates stage from a known field measurement. |
| `WATER CAL LOCK` | Locks an existing water calibration. |
| `WATER CAL UNLOCK` | Requests calibration unlock and returns the confirmation command. |
| `WATER CAL UNLOCK CONFIRM` | Unlocks calibration without erasing it. |
| `WATER CAL RESET` | Requests calibration reset and returns the confirmation command. |
| `WATER CAL RESET CONFIRM` | Clears only the water calibration. |
| `WATER RESET WATER` | Requests a water-subsystem reset and returns the confirmation command. |
| `WATER RESET WATER CONFIRM` | Resets water settings to defaults without factory-resetting Meshtastic. |
| `WATER TELEMETRY NOW` | Immediately queues a fresh water telemetry packet. |
| `WATER MODE` | Reports that this image is permanently water-only; it does not change mode. |

### HOBO / system / recovery side

| Command | What it does |
|---|---|
| `HELP` | Lists **all** HOBO, system, recovery, DFU, and water commands in six numbered messages. |
| `READ` | Requests an immediate fresh HOBO reading without consuming the automatic pointer. |
| `LOGGER` | Shows HOBO model, MAC, BLE RSSI, logging interval, target, and lock state. |
| `LOCK` / `UNLOCK` | Saves or clears the persistent HOBO assignment. |
| `AUTO` | Reports pointer-gated automatic HOBO state. |
| `STATUS` / `HEALTH` | Reports overall self-recovery health. |
| `BLE` | Reports HOBO BLE/scanner state. |
| `POWER` / `BATTERY` | Reports battery/power status. |
| `STATS` | Reports recovery counters and reset/boot information. |
| `NODES` | Reports the current Meshtastic NodeDB count. |
| `UPTIME` | Reports node uptime. |
| `VERSION` | Reports V3 firmware identity/platform. |
| `WATCHDOG` | Reports watchdog state. |
| `PING` / `WAKE` | Quick end-to-end DM/liveness checks. |
| `SCAN` | Legacy diagnostic command; V3 recovery owns BLE scanning automatically. |
| `RECONNECT` | Legacy diagnostic command; V3 recovery owns HOBO link recovery automatically. |
| `RECOVER` / `REBOOT` | Performs the safe non-destructive recovery reboot. |
| `DFU` | Arms the target for the matched drone/BLE DFU workflow. |

Commands are case-insensitive. A leading `/` is optional where supported.

## Build and flashing

Meshtastic base remains pinned to the validated **2.7.26** baseline.

Use normal RAK4631 UF2 or BLE DFU packages for routine updates. **Do not use factory-erase images for normal upgrades.**

A successful compile is required, but field deployment still requires real sensor, HOBO BLE, calibration-persistence, telemetry, and recovery testing on the actual assembled node.


## Command-reply retry protection

Normal water and HOBO command replies are sent without requesting mesh ACK/retry. This prevents one diagnostic reply from being retransmitted repeatedly when the controller-side ACK path is weak or unavailable. The firmware-update `DFU` path remains separately protected by its reliable update workflow.

The water command parser accepts only explicit `WATER ...` commands in this combined build. Bare `READ`, `STATUS`, `SENSOR`, and similar text is ignored by the water module.
