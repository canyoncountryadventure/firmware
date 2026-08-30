# RAK4631 HOBO Mesh Validation

**Branch:** `hobo-mx2001-mx2201-mx2203-rak4631`

This branch is the RAK4631 / RAK19003 HOBO mesh build. It keeps normal Meshtastic operation, reads one nearby HOBO over BLE, and automatically broadcasts successful HOBO readings as standard environmental telemetry. Custom PIR/trail-counter code is not included.

Supported logger paths:

- HOBO MX2001 — water level/stage + temperature
- HOBO MX2201 — temperature
- HOBO MX2203 — temperature
- direct Meshtastic `READ`
- automatic startup telemetry broadcast
- automatic interval discovery from the HOBO `STATUS` response
- automatic live read + telemetry broadcast at the HOBO-derived recording cadence

For logger intervals of 60 seconds or longer, the mesh cadence follows the configured HOBO recording interval exactly. Very short bench intervals are limited to one LoRa telemetry transmission per 60 seconds.

## Build

```powershell
cd C:\Meshtastic\HOBO\firmware
git fetch origin
git switch hobo-mx2001-mx2201-mx2203-rak4631
git pull --ff-only origin hobo-mx2001-mx2201-mx2203-rak4631
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e rak4631
```

Do not flash unless the build finishes with `SUCCESS`.

## Flash

The RAK4631 normally uses UF2 drag-and-drop in bootloader mode. After a successful build, locate the generated UF2 under:

```text
.pio\build\rak4631\
```

Double-reset the RAK4631 to expose its bootloader drive, then copy the UF2 to that drive.

## Hardware validation

Test one logger at a time first:

1. Expose the intended HOBO; keep other nearby HOBOs out of range/off if possible.
2. Boot the RAK4631 and confirm it remains a normal Meshtastic node.
3. Confirm the HOBO BLE connection succeeds.
4. Confirm the startup live reading is broadcast on `TELEMETRY_APP`.
5. Confirm the RAK logs a HOBO `recording interval=... s` value from `STATUS`.
6. Confirm the derived automatic telemetry cadence matches the HOBO logging interval.
7. Send a direct Meshtastic `READ` message and confirm the direct text reply.
8. Confirm the next scheduled automatic read produces another environmental telemetry packet at the expected HOBO-derived cadence.

Expected direct replies include:

```text
MX2203
Temp: 72.38 F / 22.43 C
```

```text
MX2201
Temp: 70.5 F
```

```text
MX2001
Level: 1.04 ft
Temp: 78.9 F
```

Expected serial/log messages include:

```text
RAK HOBO mesh: automatic reads follow HOBO recording interval; PIR disabled
RAK HOBO mesh: determining HOBO recording interval
RAK HOBO mesh: logger status pointer=... recording interval=... s
RAK HOBO mesh: automatic telemetry cadence=... ms from HOBO interval=... s
RAK HOBO mesh: automatic live read triggered at HOBO-derived cadence
RAK HOBO mesh: auto broadcast model=... temp=... C
```

For MX2001, confirm the standard environmental telemetry packet also contains `distance` in millimetres.

## Interval-change validation

To verify the firmware adapts without reflashing:

1. Let the RAK connect and report the current HOBO interval.
2. Change the HOBO logging interval in HOBOconnect.
3. Allow the next automatic cycle/status query to occur.
4. Confirm the RAK logs the new `recording interval=... s` and reschedules its telemetry cadence from that value.

## CCA deployment policy

- Hidden Valley: automatic remote HOBO telemetry following the HOBO recording interval.
- Home: automatic direct HOBO reading on the Heltec Home gateway.
- Fishlake: Heltec-triggered/polled `READ`, not free-running remote automatic transmission.
