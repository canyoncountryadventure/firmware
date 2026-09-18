> **Field-Recovery v2:** hardened SX1262 recovery, dual nRF52 watchdog channels, missed RX/TX IRQ polling, guarded AGC calibration, radio rail power-cycle recovery, flash-safe reboot, 12-hour burn-in reboot, and increased nRF52/BLE task stacks. Sensor behavior from RAK-HOBO-Safe-v1 is retained.\n\n# RAK HOBO Safe v1

Production HOBO-only self-recovery firmware for **RAK4631 + RAK19007/RAK19003-class WisBlock nodes**.

**Branch:** `RAK-HOBO-Safe-v2`

**Supported loggers:** Onset HOBO MX2001, MX2201, and MX2203.

This build reads real HOBO records over BLE, transmits them over Meshtastic, preserves the logger assignment across reboot, and includes non-destructive BLE/self-recovery and watchdog protection for unattended field deployment.

## Core behavior

- Uses HOBO `STATUS` / write-pointer tracking rather than a blind radio timer.
- Uses the proven `NEWREAD64` record path.
- Sends automatic telemetry only when the logger has a real new record.
- Manual `READ` does not consume the automatic pointer.
- `LOCK` persists the intended logger assignment across reboot.
- Disconnected BLE scanning uses a low-duty passive scan.
- Recovery actions preserve Meshtastic identity, channels, keys, NodeDB, and logger assignment.
- nRF52840 watchdog protection is retained without taking ownership if the core already owns the watchdog.

## Useful DM commands

Send these as a direct Meshtastic text message to the node.

| Command | What it does |
|---|---|
| `HELP` | Shows the main available command summary. |
| `STATUS` | Shows compact overall node and HOBO health. |
| `LOGGER` | Shows HOBO model, MAC, BLE RSSI, logging interval, target, and lock state. |
| `READ` | Requests a fresh HOBO measurement without consuming the automatic record pointer. |
| `LOCK` | Saves the currently identified HOBO as this station's logger. |
| `UNLOCK` | Clears the saved logger assignment and resumes discovery. |
| `BLE` | Shows BLE scan/link state, disconnected age, low-duty state, and restart count. |
| `AUTO` | Shows/validates pointer-gated automatic HOBO record operation. |
| `POWER` | Shows battery voltage, percentage, battery-present, and charging state. |
| `BATTERY` | Alias for `POWER`. |
| `STATS` | Shows recovery counts and boot/reset information. |
| `NODES` | Shows the current Meshtastic NodeDB count. |
| `UPTIME` | Shows node uptime. |
| `VERSION` | Shows firmware identity and platform. |
| `WATCHDOG` | Shows watchdog state and ownership. |
| `PING` | Quick end-to-end DM/liveness check. |
| `WAKE` | Alias for `PING`. |
| `SCAN` | Refreshes BLE scanning only when the logger is disconnected. |
| `RECONNECT` | Rebuilds the disconnected BLE scanner/link. |
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
rak4631
```

Meshtastic base remains pinned to the validated **2.7.26** baseline. Use the normal UF2 or BLE DFU package for routine updates; do not use factory-erase firmware for ordinary upgrades.
