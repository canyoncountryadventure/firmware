# RAK4631 / RAK19003

**Status:** Production / hardware validated 2026-08-19  
**Production branch:** `hobo-mx2001-mx2201-mx2203`  
**Frozen validated branch:** `hobo-universal-validated-2026-08-19`  
**PlatformIO target:** `rak4631`

This is the production RAK4631 firmware for HOBO → Meshtastic field nodes.

## Supported loggers

- MX2001 — automatic water level + temperature
- MX2201 — automatic temperature
- MX2203 — automatic temperature

## Automatic telemetry

The RAK reads the HOBO's own `STATUS` response to learn its configured logging interval and current write pointer. It waits for a real write-pointer change, then performs one fresh `NEWREAD64` and queues one Meshtastic packet.

The radio therefore follows the physical logger's actual record interval rather than running an unrelated timer.

If `STATUS` tracking fails, automatic telemetry pauses until pointer tracking recovers.

Final MX2201 hardware validation at a 20-second interval measured:

```text
count=1  pointer_to_tx=202 ms
count=2  cadence=19848 ms  pointer_to_tx=202 ms
count=3  cadence=19879 ms  pointer_to_tx=202 ms
```

## Meshtastic commands

Send direct text messages to the RAK:

```text
LOGGER
READ
LOCK
UNLOCK
```

- `LOGGER` — show model, HOBO MAC, BLE RSSI, detected interval and lock state.
- `READ` — immediate fresh read without disrupting automatic telemetry.
- `LOCK` — save the current logger MAC to flash and reconnect only to that logger after reboot.
- `UNLOCK` — clear the saved assignment and resume general discovery.

For field deployment, leave the node unlocked during bench testing. At the site, verify the intended logger with `LOGGER`, then send `LOCK`.

## Sync production

```powershell
cd C:\Meshtastic-HOBO\firmware
git fetch origin
git switch hobo-mx2001-mx2201-mx2203
git pull --ff-only origin hobo-mx2001-mx2201-mx2203
```

## Build

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e rak4631
```

## Flash

With the RAK connected by USB:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e rak4631 -t upload
```

The upload target builds first if needed, then uploads the resulting firmware.

## Serial monitor

Identify the correct COM port first:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device list
```

Then monitor it:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor --port COMx --baud 115200
```

Useful validation lines include:

```text
HOBO universal: status baseline ... interval=20 sec
HOBO universal: new logger record ...
HOBO universal TELEMETRY TX ...
HOBO universal AUTO TX confirmed ... cadence=...
```

## Persistent logger assignment

`LOCK` stores the logger address in:

```text
/prefs/hobo_lock.bin
```

The assignment was hardware-validated across a real RAK4631 reboot. When locked, non-target HOBO advertisements are ignored.

## Source files

Shared universal implementation:

```text
src/modules/Telemetry/HOBOMX2001MX2201MX2203/
├── HOBOMX2001MX2201MX2203Telemetry.cpp
├── HOBOMX2001MX2201MX2203Telemetry.h
├── HOBOMX2001MX2201MX2203TelemetryRAK.cpp
├── ONSETSDK.md
└── README.md
```

RAK board definition:

```text
variants/nrf52840/rak4631/
```

Shared nRF52 BLE setup:

```text
src/platform/nrf52/NRF52Bluetooth.cpp
```

The RAK adapter reuses the same hardware-proven universal HOBO implementation as the Seeed target.