# RAK4631 HOBO Mesh Validation

**Branch:** `hobo-mx2001-mx2201-mx2203-rak4631`

This branch is the RAK4631 / RAK19003 HOBO mesh build. It keeps normal Meshtastic operation, reads one nearby HOBO over BLE, and automatically broadcasts successful HOBO readings as standard environmental telemetry. Custom PIR/trail-counter code is not included.

Supported logger paths:

- HOBO MX2001 — water level/stage + temperature
- HOBO MX2201 — temperature
- HOBO MX2203 — temperature
- direct Meshtastic `READ`
- automatic startup telemetry broadcast
- automatic 60-minute live read + telemetry broadcast

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
5. Send a direct Meshtastic `READ` message and confirm the direct text reply.
6. Confirm the next scheduled automatic read produces another environmental telemetry packet.

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
RAK HOBO mesh: automatic reads enabled ...; PIR disabled
RAK HOBO mesh: automatic live read triggered
RAK HOBO mesh: auto broadcast model=... temp=... C
```

For MX2001, confirm the standard environmental telemetry packet also contains `distance` in millimetres.

## CCA deployment policy

- Hidden Valley: automatic remote HOBO telemetry.
- Home: automatic direct HOBO reading on the Heltec Home gateway.
- Fishlake: Heltec-triggered/polled `READ`, not free-running remote automatic transmission.
