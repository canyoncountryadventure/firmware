> **V4 recovery hardening:** this V4 target retains the proven V3 sensor behavior and adds retained nRF52 reset/failure diagnostics, bounded BLE pairing/disconnect waits, a bounded LPCOMP readiness path, and subsystem-specific breadcrumbs before watchdog recovery. Use `DIAG`/local serial to inspect retained reset context where supported.

> **CURRENT BRANCH: Seeed-HOBO-Safe-v4 — HOBO V4.** Every firmware file linked in the **V4 downloads** table below has `-v4` in its filename. The unchanged `Remote-Drone-Flashing-v2` branch hosts the **V4** compressed Scout files; its branch name is not the firmware image version. The rest of this document describes this V4 configuration (including inherited V2 safeguards).

## V4 downloads — use the file for this exact station

| File | Purpose | Link |
|---|---|---|
| `Seeed-HOBO-Safe-v4.uf2` | **V4 field radio** — normal USB UF2, for the Seeed XIAO nRF52840 node | [Download V4 target UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Seeed-HOBO-Safe-v4.uf2) |
| `Seeed-HOBO-Safe-v4-OTA.zip` | **V4 field radio** — BLE OTA update package (not a UF2) | [Download V4 target OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Seeed-HOBO-Safe-v4-OTA.zip) |
| `Drone-Seeed-HOBO-Safe-v4.uf2` | **V4 drone Scout** — flash ONLY onto the *separate RAK4631 carried by the drone*, never onto the field node | [Download matching V4 drone Scout UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Remote-Drone-Flashing-v2/downloads/Drone-Seeed-HOBO-Safe-v4.uf2) |
| `Seeed-HOBO-Safe-v4-BUILD.txt` | Target build commit and checksums | [View V4 build manifest](https://github.com/canyoncountryadventure/firmware/blob/field-self-recovery/downloads/Seeed-HOBO-Safe-v4-BUILD.txt) |
| `Drone-Seeed-HOBO-Safe-v4.uf2.txt` | Scout's embedded target branch, target commit, and checksum | [View V4 Scout manifest](https://github.com/canyoncountryadventure/firmware/blob/Remote-Drone-Flashing-v2/downloads/Drone-Seeed-HOBO-Safe-v4.uf2.txt) |

**Before a remote update:** compare the target commit in the Scout manifest with the target build manifest. If these differ, use a fresh matching Scout image; do not assume different builds are paired. For USB updates, use the normal target UF2, **not** the `Drone-` UF2.

**V4 operating behavior:** `STATUS` checks the HOBO logger write pointer on a **30-second** healthy-link cadence. The HOBO's internal logging interval is unchanged; automatic mesh telemetry is sent only on a confirmed new record. A manual `READ` remains independent. V4 inherits non-destructive field recovery and the target's existing `DFU` command.

**Source:** [Seeed-HOBO-Safe-v4](https://github.com/canyoncountryadventure/firmware/tree/Seeed-HOBO-Safe-v4) · **V4 target build:** [GitHub Actions](https://github.com/canyoncountryadventure/firmware/actions/workflows/build_hobo_v4.yml?query=branch%3ASeeed-HOBO-Safe-v4) · **V4 matching Scout build:** [GitHub Actions](https://github.com/canyoncountryadventure/firmware/actions/workflows/build_hobo_v4_drone_catalog.yml).

---

> **HOBO V4 (retaining Field-Recovery v2 safeguards):** hardened SX1262 recovery, dual nRF52 watchdog channels, missed RX/TX IRQ polling, guarded AGC calibration, radio power-cycle recovery, flash-safe reboot, 12-hour burn-in reboot, and increased nRF52/BLE task stacks. Sensor behavior from Seeed-HOBO-Safe-v1 is retained.

# Seeed HOBO Safe v4

Production HOBO-only self-recovery firmware for **Seeed XIAO nRF52840 + Wio-SX1262**.

**Branch:** `Seeed-HOBO-Safe-v4`

**Supported loggers:** Onset HOBO MX2001, MX2201, and MX2203.

This build reads real HOBO records over BLE, transmits them over Meshtastic, preserves the logger assignment across reboot, and includes non-destructive BLE/self-recovery and watchdog protection for unattended field deployment.

## Core behavior

- Uses HOBO `STATUS` / write-pointer tracking rather than a blind radio timer.
- Uses the proven `NEWREAD64` record path.
- Sends automatic telemetry only when the logger has a real new record.
- Manual `READ` does not consume the automatic pointer.
- `LOCK` persists the intended logger assignment across reboot.
- Disconnected BLE scanning uses a low-duty passive scan.
- A BLE connection attempt is cancelled after 30 seconds if it does not complete.
- Repeated `STATUS` or `NEWREAD64` failures rebuild the BLE link; five exhausted recovery cycles trip the independent field watchdog.
- Recovery actions preserve Meshtastic identity, channels, keys, NodeDB, and logger assignment.
- Field-Recovery v2 uses the normal 90-second nRF52840 watchdog plus an independent field-health watchdog channel; both run during CPU sleep/halt.

## Useful DM commands

Send these as a direct Meshtastic text message to the node.

| Command | What it does |
|---|---|
| `HELP` | Returns two numbered LoRa-safe messages containing the complete HOBO/system command set; no supported station command is intentionally omitted. |
| `STATUS` | Shows compact overall node and HOBO health. |
| `LOGGER` | Shows HOBO model, MAC, BLE RSSI, logging interval, target, and lock state. |
| `READ` | Requests a fresh HOBO measurement without consuming the automatic record pointer. |
| `LOCK` | Saves the currently identified HOBO as this station's logger. |
| `UNLOCK` | Clears the saved logger assignment and resumes discovery. |
| `BLE` | Shows central-link count, scanner state, and confirms that the HOBO state machine owns scanner/link lifecycle. |
| `AUTO` | Shows/validates pointer-gated automatic HOBO record operation. |
| `POWER` | Shows battery voltage, percentage, battery-present, and charging state. |
| `BATTERY` | Alias for `POWER`. |
| `STATS` | Shows recovery counts and boot/reset information. |
| `NODES` | Shows the current Meshtastic NodeDB count. |
| `UPTIME` | Shows node uptime. |
| `VERSION` | Shows firmware identity and platform. |
| `WATCHDOG` | Shows the 90-second core watchdog, independent field-health watchdog channel, and sleep/halt behavior. |
| `PING` | Quick end-to-end DM/liveness check. |
| `WAKE` | Alias for `PING`. |
| `SCAN` | Legacy diagnostic command. In v4 it does not manipulate the scanner; it reports that BLE recovery is automatic. |
| `RECONNECT` | Legacy diagnostic command. In v4 it does not force a link rebuild; scanner/link lifecycle remains owned by the HOBO state machine. |
| `RECOVER` | Replies, then performs a safe non-destructive reboot. |
| `REBOOT` | Alias for `RECOVER`. |

Commands are case-insensitive. A leading `/` is optional where supported.

## Recommended field check

```text
VERSION
STATUS
LOGGER
READ
LOCK
LOGGER
AUTO
BLE
WATCHDOG
POWER
PING
```

After locking the correct logger, reboot the node and verify `LOGGER` still shows the intended assignment. Then wait through at least one real HOBO logging interval and confirm an automatic packet is transmitted.

## Safe recovery rules

This firmware must not use factory erase as a recovery method. Routine recovery/reboot must preserve:

- node identity
- LoRa/channel settings and PSKs
- NodeDB
- saved HOBO logger assignment

## Build target

```text
seeed_xiao_nrf52840_kit
```

Meshtastic base remains pinned to the validated **2.7.26** baseline. Use the normal UF2 or BLE DFU package for routine updates; do not use factory-erase firmware for ordinary upgrades.
