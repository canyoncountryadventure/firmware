# DWQ Meshtastic HOBO Firmware

This repository preserves separate HOBO logger integrations built on Meshtastic firmware.

The long-lived branches are named for the **logger model**, not the radio board. Hardware such as RAK4631 or Seeed XIAO is treated as an implementation target inside each logger integration.

## Branch map

| Branch | Purpose | Currently proven hardware | Status |
|---|---|---|---|
| `mx2201-integration` | HOBO MX2201 temperature integration | Seeed XIAO nRF52840 + Wio-SX1262 | Stable / default branch |
| `mx2001-integration` | HOBO MX2001 water-level + temperature integration | RAK19003 + RAK4631 | Finalized working branch |
| `mx2201-newread-test` | Historical MX2201 NEWREAD64 test work | Seeed XIAO nRF52840 + Wio-SX1262 | Historical only; do not deploy from this branch |

If a logger is later ported to another board, keep the same integration branch name. Board-specific development can use temporary feature branches such as `mx2201-rak4631` or `mx2001-seeed-xiao` until validated.

---

# HOBO MX2201 Meshtastic Integration

This default branch contains the bench-proven HOBO MX2201 integration for the Seeed XIAO nRF52840 + Wio-SX1262 Meshtastic node.

**Current stable firmware tag:** `mx2201-stable-newread-2026-08-13`  
**Current stable firmware commit:** `38891aa8e13708ce97de1d3bb4c493eafecb2886`  
**Legacy rollback tag:** `mx2201-stable-2026-08-13`  
**Meshtastic base:** `2.7.26` at `54e0d8d0ab2ff56b3a9ce967e53f79e49af560fb`  
**Currently proven PlatformIO target:** `seeed_xiao_nrf52840_kit`  
**Full technical documentation:** [MX2201_INTEGRATION.md](MX2201_INTEGRATION.md)

## Current proven behavior

The stable firmware has been physically bench-tested end-to-end:

```text
HOBO MX2201
    ↓ BLE
Seeed XIAO nRF52840 + Wio-SX1262
    ↓ standard Meshtastic EnvironmentMetrics.temperature
LoRa mesh
    ↓
second Meshtastic node
    ↓ BLE
Meshtastic app
```

The current implementation uses Onset's direct `NEWREAD64` live-sensor command for active temperature acquisition.

Proven behavior includes:

- autonomous MX2201 discovery and connection after node reboot
- direct live temperature acquisition using `NEWREAD64`
- proven temperature parsing/calibration
- logger status polling to maintain the BLE session and read the configured logging interval
- one fresh direct read after connection
- a fresh direct read before each scheduled telemetry transmission
- logger intervals of 60 seconds or longer followed by the mesh reporting schedule
- short bench intervals clamped to a 60-second minimum mesh-report interval to avoid LoRa flooding
- standard Meshtastic environmental telemetry; no custom payload required for MX2201 temperature

The one-hour logger test was physically verified with `interval=3600 seconds`. The node sent a fresh startup temperature and did not repeatedly retransmit cached temperature data during the test window.

## Recovery points

Current stable recovery point:

```powershell
git fetch origin --tags
git checkout mx2201-stable-newread-2026-08-13
```

Previous stable rollback:

```powershell
git checkout mx2201-stable-2026-08-13
```

Normal MX2201 development branch:

```powershell
git checkout mx2201-integration
git pull origin mx2201-integration
```

Stable tags are immutable recovery points. Do not move or overwrite them.

## Local workspace recommendation

Keep separate local clones/workspaces:

```text
C:\Meshtastic\MX2001\firmware   -> mx2001-integration
C:\Meshtastic\MX2201\firmware   -> mx2201-integration
```

This avoids accidentally compiling or flashing the wrong logger integration.

---

## Upstream Meshtastic

This repository is based on Meshtastic device firmware, an open-source LoRa mesh networking project for long-range, low-power communication without relying on internet or cellular infrastructure.

- [Meshtastic firmware build documentation](https://meshtastic.org/docs/development/firmware/build)
- [Meshtastic flashing documentation](https://meshtastic.org/docs/getting-started/flashing-firmware/)
