# HOBO MX2001 → Meshtastic Water-Level Integration

This branch contains the bench-proven **HOBO MX2001 water-level + temperature integration for a RAK4631 Meshtastic node**.

The working system reads the MX2001 over Bluetooth, follows the logger's actual configured logging interval, detects each newly logged record, reads current **water level (WL)** and **temperature**, and broadcasts a compact custom packet through the normal Meshtastic LoRa mesh.

**Integration branch:** `mx2001-rak`  
**Bench-proven firmware commit:** `ada4c9526e68819506a44bf171aa5dff3de59660`  
**Meshtastic build:** `2.7.26.ded77c2`  
**PlatformIO target:** `rak4631`  
**Field hardware:** RAK19003 + RAK4631  
**Technical reference:** [MX2001_INTEGRATION.md](MX2001_INTEGRATION.md)  
**Windows receiver:** [tools/mx2001_receiver.py](tools/mx2001_receiver.py)

> The stable MX2201 work is preserved separately on `mx2201-integration`. This branch is the MX2001/RAK work and does not replace the MX2201 recovery branch.

## Proven end-to-end path

```text
HOBO MX2001
    |
    | BLE GATT
    v
RAK19003 + RAK4631 field node
    |
    | Meshtastic PRIVATE_APP packet (port 256)
    | LoRa / normal Meshtastic routing and relays
    v
Any Meshtastic receiver on the same channel/PSK
    |
    | USB serial
    v
mx2001_receiver.py
    |
    +--> readable WL + temperature
    +--> BLE RSSI + LoRa RSSI/SNR
    +--> CSV log
```

## Current proven behavior

The full chain has been physically bench-tested.

- automatic MX2001 discovery; no hard-coded logger MAC is required
- BLE central connection from the RAK4631 while normal Meshtastic BLE remains available
- Onset service/command characteristic discovery
- logger status reading, including the **actual configured logging interval**
- 20-second logger setting read directly as `interval=20`
- write-pointer monitoring to identify actual new logger records
- direct `NEWREAD64` acquisition of current temperature and water level
- water level decoded directly from the MX2001's own WL value
- one startup snapshot after connection
- one mesh data packet for each subsequent new logger record
- normal Meshtastic routing/rebroadcast behavior
- successful reception and decoding by a second radio connected to a Windows PC
- CSV logging on the receiver

A production bench run showed:

```text
Interval: 20 seconds
Pointer: 0x000016A0

startup:     WL 0.919 ft   Temp 80.29 F
next record: WL 0.913 ft   Temp 80.29 F
next record: WL 0.905 ft   Temp 80.29 F
```

The pointer moved with the logger records:

```text
0x16A0 -> 0x16A7
0x16A7 -> 0x16AE
```

Each new record produced exactly one custom Meshtastic transmission.

A separate receiving radio then decoded a later packet as:

```text
Mesh source: !b57d051f
Logger:      F1:0D:9D:29:C3:2D
Sequence:    29
Water level: 0.9 ft
Temperature: 80.5 F
BLE RSSI:    -65 dBm
LoRa RSSI:   -80 dBm
LoRa SNR:    6.5 dB
```

That test proves the complete **MX2001 → BLE → field RAK → LoRa mesh → second radio → PC** path.

## Why a custom Meshtastic packet is used

Stock Meshtastic environmental telemetry does not provide the desired feet-based water-level field. This integration therefore uses:

```text
PRIVATE_APP = 256
```

with a small 19-byte payload containing:

- packet/version marker
- measurement sequence
- WL in tenths of a foot
- temperature in tenths of °F
- raw MX2001 temperature value
- MX2001 BLE MAC
- BLE RSSI

A normal Meshtastic radio can receive and relay this packet without custom firmware. The custom decoder is only required at the endpoint where the bytes are turned into readable measurements.

## Build

From the firmware repository root on Windows:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e rak4631
```

Expected UF2 name for this pinned build:

```text
.pio\build\rak4631\firmware-rak4631-2.7.26.ded77c2.uf2
```

Flash to a RAK4631 in bootloader mode:

```powershell
$rak = (Get-Volume | Where-Object FileSystemLabel -eq "RAK4631").DriveLetter
Copy-Item ".pio\build\rak4631\firmware-rak4631-2.7.26.ded77c2.uf2" "$($rak):\"
```

## Windows receiver

Install the Python dependencies once:

```powershell
py -m pip install --upgrade meshtastic pypubsub
```

Find the receiving radio COM port, then run:

```powershell
py .\tools\mx2001_receiver.py --port COM5
```

The receiving radio only needs to be a normal Meshtastic node on the same channel/key. It does **not** need the MX2001 firmware.

The script writes:

```text
mx2001_data.csv
```

in the directory where the script is launched.

## Source files intentionally changed

```text
src/modules/Telemetry/MX2001Diagnostic.cpp
src/modules/Telemetry/MX2001Diagnostic.h
src/modules/Modules.cpp
src/platform/nrf52/NRF52Bluetooth.cpp
```

The `MX2001Diagnostic` filename is historical from protocol-development work. The code at the stable commit is the production sender that passed the end-to-end bench test. It is intentionally left under the tested filename for this recovery point rather than being renamed during stabilization.

## Development rule

Treat `ada4c9526e68819506a44bf171aa5dff3de59660` as the first bench-proven MX2001 production sender recovery point. Future refactors should be separate commits and should not overwrite the stable MX2201 branch.

---

## Upstream Meshtastic

This repository is based on the open-source Meshtastic device firmware. Upstream build and flashing documentation is available at the Meshtastic project documentation site.
