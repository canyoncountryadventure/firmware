# RAK4631 Universal HOBO Validation

**Branch:** `hobo-mx2001-mx2201-mx2203-rak4631`

**Status:** RAK4631 hardware-validation branch. The Seeed universal branch remains untouched and hardware-proven.

This branch ports the same universal HOBO protocol implementation to the RAK4631 / RAK19003 target.

Supported logger protocol paths to validate:

- HOBO MX2001 — water level + temperature
- HOBO MX2201 — temperature
- HOBO MX2203 — temperature
- direct Meshtastic message `READ`

The nRF52 Bluetooth layer already reserves one peripheral link for the Meshtastic phone and one central link for the HOBO logger on `RAK_4631`.

## Build

```powershell
cd C:\Meshtastic\HOBO\firmware
git fetch origin
git switch -c hobo-mx2001-mx2201-mx2203-rak4631 --track origin/hobo-mx2001-mx2201-mx2203-rak4631
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e rak4631
```

If the local branch already exists:

```powershell
git switch hobo-mx2001-mx2201-mx2203-rak4631
git pull --ff-only origin hobo-mx2001-mx2201-mx2203-rak4631
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e rak4631
```

Do not flash unless the build finishes with `SUCCESS`.

## Flash

The RAK4631 normally uses UF2 drag-and-drop in bootloader mode. After a successful build, locate the newest generated UF2 under:

```text
.pio\build\rak4631\
```

Double-reset the RAK4631 to expose its bootloader drive, then copy the generated UF2 to that drive.

## Hardware test

Test one logger at a time first:

1. MX2203 exposed; other HOBOs covered/off.
2. MX2201 exposed; other HOBOs covered/off.
3. MX2001 exposed; other HOBOs covered/off.
4. After each BLE connection is established, send a direct Meshtastic `READ` message to the RAK node.

Expected replies:

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

After all three pass individually, expose all three simultaneously to verify candidate selection/reconnection behavior.

Once the RAK passes this physical validation, fold the RAK target into the canonical `hobo-mx2001-mx2201-mx2203` production branch and archive this validation branch.
