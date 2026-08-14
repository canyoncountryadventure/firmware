# DWQ MX2201 Meshtastic Integration

This repository contains the bench-proven HOBO MX2201 integration for the Seeed XIAO nRF52840 + Wio-SX1262 Meshtastic node.

**Current stable firmware:** `mx2201-stable-newread-2026-08-13`  
**Current stable commit:** `38891aa8e13708ce97de1d3bb4c493eafecb2886`  
**Legacy rollback tag:** `mx2201-stable-2026-08-13`  
**Meshtastic base:** `2.7.26` at `54e0d8d0ab2ff56b3a9ce967e53f79e49af560fb`  
**PlatformIO target:** `seeed_xiao_nrf52840_kit`  
**Full technical documentation:** [MX2201_INTEGRATION.md](MX2201_INTEGRATION.md)

## Current proven behavior

The current stable firmware has been bench-tested end-to-end:

```text
HOBO MX2201
    -> BLE
DWQ Data Node / XIAO nRF52840 + Wio-SX1262
    -> standard Meshtastic EnvironmentMetrics.temperature
    -> LoRa mesh
second Meshtastic node
    -> BLE
Android Meshtastic app
```

The current implementation uses Onset's direct **NEWREAD64** live-sensor command instead of relying on the MX2201 historical-memory decoder for active temperature acquisition.

Key proven behavior:

- autonomous MX2201 discovery and connection after node reboot; no logger button press required in the tested setup
- direct live temperature acquisition using `NEWREAD64`
- exact response parsing from bytes 8..11 as a big-endian temperature raw value
- unchanged, bench-proven MX2201 temperature calibration
- logger status polled at most every 10 seconds to maintain the BLE session and read the current logging interval
- write-pointer changes are diagnostic only and do **not** trigger telemetry
- one fresh `NEWREAD64` read immediately after connection
- scheduled telemetry obtains a fresh `NEWREAD64` reading before each transmission
- for logger intervals of 60 seconds or longer, Meshtastic temperature reporting follows the MX2201 logging interval
- short bench logger intervals are clamped to a 60-second minimum mesh-report interval to avoid flooding LoRa
- no repeated one-minute retransmission of a cached temperature when the logger is configured for one-hour logging
- standard Meshtastic telemetry only; no proprietary packet type

The one-hour logger test was physically verified with `interval=3600 seconds`. The node acquired and transmitted a fresh startup temperature, continued 10-second status checks, ignored a subsequent write-pointer change for telemetry purposes, and did not send another cached temperature during the following test window.

## Recovery points

Current production recovery point:

```powershell
git fetch origin --tags
git checkout mx2201-stable-newread-2026-08-13
```

Previous stable implementation, preserved unchanged for rollback:

```powershell
git checkout mx2201-stable-2026-08-13
```

Normal development branch:

```powershell
git checkout mx2201-integration
git pull origin mx2201-integration
```

Stable tags are immutable recovery points. Do not move or overwrite them.

---

<div align="center" markdown="1">

<img src=".github/meshtastic_logo.png" alt="Meshtastic Logo" width="80"/>
<h1>Meshtastic Firmware</h1>

![GitHub release downloads](https://img.shields.io/github/downloads/meshtastic/firmware/total)
[![CI](https://img.shields.io/github/actions/workflow/status/meshtastic/firmware/main_matrix.yml?branch=master&label=actions&logo=github&color=yellow)](https://github.com/meshtastic/firmware/actions/workflows/ci.yml)
[![CLA assistant](https://cla-assistant.io/readme/badge/meshtastic/firmware)](https://cla-assistant.io/meshtastic/firmware)
[![Fiscal Contributors](https://opencollective.com/meshtastic/tiers/badge.svg?label=Fiscal%20Contributors&color=deeppink)](https://opencollective.com/meshtastic/)
[![Vercel](https://img.shields.io/static/v1?label=Powered%20by&message=Vercel&style=flat&logo=vercel&color=000000)](https://vercel.com?utm_source=meshtastic&utm_campaign=oss)

<a href="https://trendshift.io/repositories/5524" target="_blank"><img src="https://trendshift.io/api/badge/repositories/5524" alt="meshtastic%2Ffirmware | Trendshift" style="width: 250px; height: 55px;" width="250" height="55"/></a>

</div>

<div align="center">
	<a href="https://meshtastic.org">Website</a>
	-
	<a href="https://meshtastic.org/docs/">Documentation</a>
</div>

## Upstream Meshtastic overview

This repository is based on Meshtastic device firmware, an open-source LoRa mesh networking project for long-range, low-power communication without relying on internet or cellular infrastructure. The firmware supports multiple hardware platforms including nRF52, ESP32, RP2040/RP2350, and Linux devices.

### Upstream documentation

- **[Building Instructions](https://meshtastic.org/docs/development/firmware/build)**
- **[Flashing Instructions](https://meshtastic.org/docs/getting-started/flashing-firmware/)**
