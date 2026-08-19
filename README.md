# HOBO MX2001 → Meshtastic Water-Level Integration

This branch contains the finalized **HOBO MX2001 water-level + temperature integration** for Meshtastic.

The integration is named for the **logger**, not the radio board. The currently proven field implementation uses a RAK19003 + RAK4631, but future Seeed/XIAO or other supported-board ports belong under this same MX2001 integration rather than requiring the project itself to be renamed.

## Branch status

- **Branch:** `mx2001-integration`
- **Finalized firmware code baseline:** `93f03548678829c485d8327f468e91fc3ee0c4b1`
- **Meshtastic build base:** `2.7.26.ded77c2`
- **Currently proven PlatformIO target:** `rak4631`
- **Currently proven field hardware:** RAK19003 + RAK4631
- **Technical reference:** [MX2001_INTEGRATION.md](MX2001_INTEGRATION.md)
- **Tracked Windows receiver:** [tools/mx2001_receiver.py](tools/mx2001_receiver.py)

The MX2201 work is preserved separately on `mx2201-integration`.

## Naming rule

The long-lived integration branches are logger-centric:

```text
mx2001-integration
mx2201-integration
```

Hardware is treated as an implementation target inside the logger integration. If board-specific development later needs isolation, use temporary or feature branches such as:

```text
mx2001-rak4631
mx2001-seeed-xiao
mx2201-rak4631
mx2201-seeed-xiao
```

After validation, supported hardware can be incorporated into the appropriate logger integration branch.

## Proven behavior

The physical end-to-end path has been tested:

```text
HOBO MX2001
    ↓ BLE
RAK19003 + RAK4631 field node
    ↓ Meshtastic PRIVATE_APP / port 256
LoRa mesh / normal Meshtastic relays
    ↓
Meshtastic receiver
    ↓ USB / gateway
receiver + database/dashboard
```

The field firmware currently provides:

- automatic MX2001 discovery and BLE connection
- logger STATUS decoding for the actual configured logging interval and write pointer
- `NEWREAD64` acquisition of current water level and temperature
- water level decoded from the MX2001 WL value and converted to feet
- temperature decoded from the proven MX2001 raw-temperature field
- one startup snapshot after connection
- one compact 19-byte mesh packet for each new logger record
- normal Meshtastic routing/rebroadcast through unmodified relay nodes
- on-demand direct-message command: `READ`
- `READ` reply containing current level and temperature

Example DM reply:

```text
Level: 1.04 ft
Temp: 78.9 F
```

A genuine HOBO logger timestamp is **not** included yet. Protocol testing did not identify a verified logger-clock timestamp in `NEWREAD64`, so the production firmware intentionally does not substitute Meshtastic, phone, gateway, or receive time and label it as HOBO time.

## Custom telemetry packet

The automatic sensor packet uses:

```text
PRIVATE_APP = 256
```

Payload length: **19 bytes**.

It contains:

- version/marker
- measurement sequence
- water level in tenths of a foot
- temperature in tenths of °F
- raw MX2001 temperature value
- logger BLE MAC
- BLE RSSI

Normal Meshtastic nodes can relay this payload without custom firmware. A decoder is required only at the endpoint that turns the private payload into readable measurements.

## Build — currently proven RAK4631 target

From the firmware repository root:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e rak4631
```

Flash the newest generated UF2 after placing the RAK4631 in bootloader mode:

```powershell
Copy-Item (Get-ChildItem ".pio\build\rak4631\*.uf2" | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName "E:\"
```

The bootloader drive letter can vary.

## Windows receiver

Install dependencies once:

```powershell
py -m pip install --upgrade meshtastic pypubsub
```

Run the tracked receiver from the repository:

```powershell
py .\tools\mx2001_receiver.py --port COM5
```

COM ports vary by device/computer.

## Source files intentionally changed

```text
src/modules/Telemetry/MX2001Diagnostic.cpp
src/modules/Telemetry/MX2001Diagnostic.h
src/modules/Modules.cpp
src/platform/nrf52/NRF52Bluetooth.cpp
```

The `MX2001Diagnostic` filename is historical from protocol-development work. It remains unchanged because the working firmware has been physically tested under that implementation.

## Development rule

Treat `93f03548678829c485d8327f468e91fc3ee0c4b1` as the finalized firmware-code baseline for the current MX2001 implementation. Documentation commits may follow it without changing the proven firmware.

Future functional changes should be new commits on `mx2001-integration` and should be physically validated before replacing this baseline.

## Recommended local workspace

```text
C:\Meshtastic\MX2001\firmware   -> mx2001-integration
```

---

## Upstream Meshtastic

This repository is based on the open-source Meshtastic device firmware. Upstream build and flashing documentation is available from the Meshtastic project.
