# Seeed XIAO nRF52840 + Wio-SX1262

**Status:** Production / hardware validated 2026-08-19  
**Production branch:** `hobo-mx2001-mx2201-mx2203`  
**Frozen validated branch:** `hobo-universal-validated-2026-08-19`  
**PlatformIO target:** `seeed_xiao_nrf52840_kit`

This is the production Seeed firmware for HOBO → Meshtastic field nodes.

## Supported loggers

- MX2001 — automatic water level + temperature
- MX2201 — automatic temperature
- MX2203 — automatic temperature

## Automatic telemetry

The Seeed uses the same shared universal HOBO implementation as the RAK4631.

It reads the HOBO `STATUS` response, learns the configured logging interval and current write pointer, waits for the write pointer to advance, then performs one fresh `NEWREAD64` and queues one Meshtastic packet.

Automatic data therefore follows the HOBO's actual logging interval rather than an independent radio timer. If `STATUS` tracking fails, automatic telemetry pauses until pointer tracking recovers.

## Meshtastic commands

Send direct text messages to the node:

```text
LOGGER
READ
LOCK
UNLOCK
```

- `LOGGER` — show model, HOBO MAC, BLE RSSI, detected interval and lock state.
- `READ` — immediate fresh read without disrupting automatic telemetry.
- `LOCK` — save the current logger MAC to flash and reconnect only to that logger after reboot.
- `UNLOCK` — clear the assignment and resume discovery of any supported HOBO.

For field deployment, leave radios unlocked during bench testing. At the monitoring site, use `LOGGER` to verify the intended logger, then send `LOCK`.

## Sync production

```powershell
cd C:\Meshtastic-HOBO\firmware
git fetch origin
git switch hobo-mx2001-mx2201-mx2203
git pull --ff-only origin hobo-mx2001-mx2201-mx2203
```

## Build

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e seeed_xiao_nrf52840_kit
```

## Flash

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e seeed_xiao_nrf52840_kit -t upload
```

The upload target builds first if needed, then uploads the resulting firmware.

## Serial monitor

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device list
```

Then:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor --port COMx --baud 115200
```

Useful validation lines include:

```text
HOBO universal: status baseline ... interval=...
HOBO universal: new logger record ...
HOBO universal TELEMETRY TX ...
HOBO universal AUTO TX confirmed ... cadence=...
```

## Persistent logger assignment

`LOCK` stores the target BLE address in:

```text
/prefs/hobo_lock.bin
```

When locked, only the saved HOBO is eligible for connection. If that logger is unavailable, the node waits for it instead of switching to another nearby logger.

## Hardware validation

The universal Seeed firmware has been physically tested against MX2001, MX2201, and MX2203 without reflashing between logger models and successfully maintained one HOBO BLE connection at a time.

The final record-aligned scheduler and persistent lock use the same shared implementation that was cadence-validated on the RAK4631.

## Source files

```text
src/modules/Telemetry/HOBOMX2001MX2201MX2203/
├── HOBOMX2001MX2201MX2203Telemetry.cpp
├── HOBOMX2001MX2201MX2203Telemetry.h
├── ONSETSDK.md
└── README.md
```

Seeed board definition:

```text
variants/nrf52840/seeed_xiao_nrf52840_kit/
```

Shared nRF52 BLE setup:

```text
src/platform/nrf52/NRF52Bluetooth.cpp
```

The Seeed build intentionally reuses the same universal protocol implementation rather than maintaining a separate logger behavior.