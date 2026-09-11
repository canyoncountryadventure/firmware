# HOBO Peak Node — Seeed Self-Recovery Firmware

![Board](https://img.shields.io/badge/board-Seeed%20XIAO%20nRF52840-0A7F5A)
![Radio](https://img.shields.io/badge/radio-Wio--SX1262%20%2F%20Meshtastic-6A5ACD)
![HOBO](https://img.shields.io/badge/HOBO-MX2001%20%7C%20MX2201%20%7C%20MX2203-1F6FEB)
![Recovery](https://img.shields.io/badge/self--recovery-enabled-success)
![Watchdog](https://img.shields.io/badge/watchdog-15%20minute%20nRF52-orange)
![BLE](https://img.shields.io/badge/BLE%20scan-10%25%20duty-informational)

> **Branch:** `hobo-self-recovery-seeed`
>
> **Target:** Seeed XIAO nRF52840 Kit + Wio-SX1262
>
> **Mission:** read an Onset HOBO logger, transmit every real logged record over Meshtastic, survive unattended operation, and give you enough remote commands to avoid hiking back to a peak for routine failures.

This branch is the hardened Seeed field-node build for remote environmental stations. It keeps the proven universal HOBO protocol implementation intact and adds a separate non-destructive recovery supervisor around it.

The design goal is simple:

**If the logger, BLE stack, or firmware has a recoverable problem, the node should either fix itself or tell you enough over Meshtastic to fix it remotely.**

---

## Architecture

```text
┌───────────────────────────┐
│ Onset HOBO logger         │
│ MX2001 / MX2201 / MX2203 │
└─────────────┬─────────────┘
              │ BLE
              │ STATUS + NEWREAD64
              ▼
┌───────────────────────────┐
│ Seeed XIAO nRF52840       │
│ + Wio-SX1262              │
│                           │
│ Universal HOBO reader     │
│ Self-recovery supervisor  │
│ DM diagnostics            │
│ Persistent logger lock    │
└─────────────┬─────────────┘
              │ Meshtastic
              ▼
┌───────────────────────────┐
│ Mesh / gateway / cloud    │
└───────────────────────────┘
```

There is **no blind radio timer pretending to be the logger interval**. The node follows the HOBO itself.

---

# Core guarantees

This branch is intentionally conservative about the things that matter in the field.

- Supports **MX2001, MX2201, and MX2203**.
- Uses the proven HOBO `NEWREAD64` path.
- Uses HOBO `STATUS` to track the logger write pointer.
- Learns and follows the logger's configured interval.
- Sends one automatic packet only after a **real new logger record** appears.
- A manual `READ` does **not** consume the automatic pointer.
- Automatic telemetry pauses if pointer tracking becomes unreliable rather than fabricating records.
- `LOCK` persists the intended HOBO assignment across reboot.
- Recovery actions preserve Meshtastic configuration and logger assignment.
- No recovery path erases NVS, NodeDB, channels, PSKs, node identity, or the saved HOBO lock.
- The recovery supervisor is separate from the logger decoder so recovery changes do not rewrite the proven HOBO protocol logic.

---

## Supported HOBO models

| Logger | Automatic telemetry | Manual `READ` | Primary use |
|---|---|---|---|
| **MX2001** | Water level + temperature | Water level + temperature | Water-level stations |
| **MX2201** | Temperature | Temperature | Air/water temperature |
| **MX2203** | Temperature | Temperature | Temperature stations |

---

# How automatic telemetry actually works

```text
CONNECT TO LOCKED/VALID HOBO
          │
          ▼
      HOBO STATUS
          │
          ├── learn logger interval
          └── learn write pointer
          │
          ▼
  wait for pointer advance
          │
          ▼
     NEW RECORD EXISTS
          │
          ▼
       NEWREAD64
          │
          ▼
 decode measurement
          │
          ▼
 queue Meshtastic packet
          │
          ▼
 consume pointer only after
 successful packet queue
```

This is important: **automatic telemetry follows recorded HOBO data, not radio uptime.**

If three consecutive `STATUS` attempts fail, automatic transmission pauses until pointer tracking recovers. Manual `READ` can still be useful for diagnostics because the manual path is intentionally independent of the automatic pointer.

---

# Self-recovery system

The supervisor handles failures outside the HOBO parser without destructively changing configuration.

| Condition | Automatic behavior |
|---|---|
| Normal operation | Feed watchdog and leave healthy HOBO link alone |
| Searching for HOBO | Passive low-duty BLE scanning |
| Disconnected scanner running for 30 min | Refresh BLE scanner |
| No HOBO link for 6 hours | Safe MCU reboot to rebuild BLE stack |
| Main scheduler / firmware stalls | nRF52840 hardware watchdog resets node |
| User sends `SCAN` | Refresh scanner only when disconnected |
| User sends `RECONNECT` | Rebuild disconnected scanner |
| User sends `RECOVER` or `REBOOT` | Safe reboot after reply is sent |

### Hardware watchdog

The nRF52840 watchdog is armed for approximately **15 minutes** and configured to continue running during CPU sleep.

The supervisor will **not take ownership of the watchdog if another part of the board/core already owns it**.

### BLE power discipline

The original scanner configuration was comparatively aggressive. This branch uses:

```text
Interval: 320 units = 200 ms
Window:    32 units =  20 ms
Approximate receive duty: 10%
Passive scan: enabled
```

That matters on solar/battery stations that may spend long periods searching for a logger.

A healthy active HOBO connection is not periodically torn down just to satisfy the recovery supervisor.

---

# Direct-message command center

Send commands as a **direct Meshtastic text message to the node**. Commands are case-insensitive and a leading `/` is optional where supported by the parser.

## Logger commands

| Command | Result |
|---|---|
| `READ` | Fresh logger measurement without consuming the automatic pointer |
| `LOGGER` | Model, logger MAC, BLE RSSI, interval, lock state, target logger |
| `LOCK` | Persist the currently identified logger as this station's logger |
| `UNLOCK` | Clear the persistent logger assignment and resume discovery |

## Health and diagnostics

| Command | Result |
|---|---|
| `STATUS` | Compact overall node health report |
| `HEALTH` | Alias for `STATUS` |
| `POWER` | Battery voltage, percentage, battery-present and charging status |
| `BATTERY` | Alias for `POWER` |
| `BLE` | BLE scanning/link state, disconnected age, low-duty state, restart count |
| `AUTO` | Confirms pointer-gated automatic NEWREAD operation |
| `STATS` | Recovery counts and boot reset reason |
| `NODES` | Current Meshtastic NodeDB count |
| `UPTIME` | Node uptime in seconds |
| `VERSION` | Firmware identity and platform |
| `WATCHDOG` | Watchdog running/ownership state |
| `PING` | Fast end-to-end packet/liveness check |
| `WAKE` | Alias for `PING` |
| `HELP` | Core command summary |

## Recovery commands

| Command | Result |
|---|---|
| `SCAN` | Refresh disconnected BLE scanning without disturbing an apparent active link |
| `RECONNECT` | Rebuild the scanner when disconnected |
| `RECOVER` | Reply, then perform a safe non-destructive reboot |
| `REBOOT` | Alias for `RECOVER` |

## Scientific field-validation subsystem

For extremely rigorous packet-return verification:

| Command | Response |
|---|---|
| `AMY` | `is a little bitch` |
| `CRESSTON` | `is a little bitch` |

These two commands are deliberately implemented in a tiny isolated text-message module. They do not touch the HOBO state machine, automatic pointer, logger lock, watchdog, or BLE recovery logic.

---

# Recommended field deployment sequence

Before walking away from a station:

1. Flash the **Seeed self-recovery** build.
2. Confirm the node boots and appears in Meshtastic.
3. DM `VERSION`.
4. DM `STATUS`.
5. DM `LOGGER` and verify the intended HOBO model/MAC.
6. DM `READ` and compare the value with the HOBO app/logger.
7. DM `LOCK` only after confirming the correct physical logger.
8. Reboot the radio.
9. DM `LOGGER` again and verify the lock persisted.
10. Wait through at least one actual logger interval.
11. Confirm one automatic packet appears after the HOBO creates its next record.
12. DM `AUTO`.
13. DM `BLE`.
14. DM `WATCHDOG`.
15. DM `POWER`.
16. DM `PING` from the radio/gateway you expect to use remotely.
17. If you want the unofficial test, DM `AMY` or `CRESSTON` and confirm the reply comes back.

Do not call a deployment complete until the **automatic** record path has been observed. A successful manual `READ` proves BLE reading and decoding; it does not by itself prove automatic pointer tracking.

---

# Remote troubleshooting ladder

Use the least invasive tool first.

```text
PING
 ↓
STATUS
 ↓
LOGGER
 ↓
READ
 ↓
BLE
 ↓
SCAN             (only refresh disconnected scanner)
 ↓
RECONNECT        (rebuild disconnected scanner)
 ↓
RECOVER          (safe reboot)
```

If `READ` works but automatic records stop, investigate `STATUS`/pointer tracking before assuming the logger decoder is broken.

If the radio stops answering entirely, the hardware watchdog is the final automatic recovery layer.

---

# Things this branch must never do

Do **not** turn field recovery into destructive recovery.

- Do not add automatic flash erasure.
- Do not clear NVS as a recovery strategy.
- Do not clear NodeDB.
- Do not regenerate node identity.
- Do not silently replace channel keys.
- Do not silently change LoRa channel/slot/frequency settings.
- Do not make manual `READ` advance the automatic pointer.
- Do not replace HOBO write-pointer tracking with a blind timer.
- Do not transmit invented measurements when logger status is uncertain.
- Do not make the recovery supervisor routinely disconnect a healthy HOBO link.

---

# Build target

PlatformIO / Meshtastic environment:

```text
seeed_xiao_nrf52840_kit
```

GitHub Actions workflow:

```text
.github/workflows/build_hobo_self_recovery_seeed.yml
```

Every push to this branch builds the Seeed target and uploads a firmware artifact. Treat a green workflow as the minimum software gate; bench testing with a real logger remains the hardware gate.

---

# Code map

```text
src/modules/Telemetry/
│
├── HOBOMX2001MX2201MX2203/
│   └── universal HOBO STATUS / NEWREAD64 / decoding / AUTO logic
│
├── HOBOMX2201MX2001/
│   └── Seeed Meshtastic compatibility hook
│
└── HOBOSelfRecovery/
    ├── HOBOSelfRecovery.cpp
    ├── HOBOSelfRecovery.h
    ├── HOBOFieldCheck.cpp
    └── HOBOFieldCheck.h
```

The design deliberately separates **measurement correctness** from **node survivability**.

---

# Production philosophy

A remote monitoring radio is not useful because it worked on the bench once. It is useful because it can sit on a ridge, tower, drainage, or peak for months and continue doing the boring thing correctly.

This branch is built around that idea:

> **Read the logger only when the logger has new data. Preserve the station's identity and configuration. Recover from transient failures automatically. Expose enough remote diagnostics that a mountain hike is the last troubleshooting step, not the first.**

---

## Related branches

| Branch | Purpose |
|---|---|
| `hobo-self-recovery-seeed` | **This build — hardened Seeed XIAO field node** |
| `hobo-self-recovery-rak4631` | Hardened RAK4631 field node |
| `hobo-mx2001-mx2201-mx2203` | Universal HOBO source branch from which these hardened builds were derived |
| `cca-heltec-sensor-gateway` | Heltec V4 internet/cloud gateway |

For the universal HOBO protocol details, see:

- [`src/modules/Telemetry/HOBOMX2001MX2201MX2203/README.md`](src/modules/Telemetry/HOBOMX2001MX2201MX2203/README.md)
- [`Meshtastic/SHARED-HOBO/README.md`](Meshtastic/SHARED-HOBO/README.md)
- [`Meshtastic/SEEED-XIAO/README.md`](Meshtastic/SEEED-XIAO/README.md)
