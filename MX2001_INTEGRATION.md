# MX2001 Meshtastic Integration — Technical Reference

This document preserves the protocol facts, decode rules, packet format, state machine, hardware target, and bench evidence for the HOBO MX2001 integration on a RAK4631 Meshtastic node.

Its purpose is to prevent solved reverse engineering from being repeated and to provide a known recovery reference before future field and dashboard work.

## Stable reference

- Integration branch: `mx2001-rak`
- Bench-proven production sender commit: `ada4c9526e68819506a44bf171aa5dff3de59660`
- Meshtastic build identity during the successful test: `2.7.26.ded77c2`
- Base branch point: `ded77c2f0f4c8477eb56d0612053c77f6765084c`
- PlatformIO environment: `rak4631`
- Hardware: RAK19003 base + RAK4631 nRF52840/SX1262 core
- Test logger BLE MAC: `F1:0D:9D:29:C3:2D`

The MX2201 integration remains separately preserved on `mx2201-integration`.

## Proven architecture

```text
MX2001 pressure/temperature logger
        |
        | BLE
        v
RAK4631 field node
        |
        | PRIVATE_APP / port 256
        | normal Meshtastic LoRa routing
        v
relay(s), if present
        |
        v
normal Meshtastic receiver
        |
        | USB serial
        v
Python receiver / future gateway
        |
        +--> CSV
        +--> database/dashboard (future)
```

The receiving radio requires no custom MX2001 firmware. It only receives and forwards the custom Meshtastic payload to the connected computer.

## Source scope

The stable MX2001 integration changes only:

```text
src/modules/Telemetry/MX2001Diagnostic.cpp
src/modules/Telemetry/MX2001Diagnostic.h
src/modules/Modules.cpp
src/platform/nrf52/NRF52Bluetooth.cpp
```

`Modules.cpp` adds the MX2001 module only for `ARCH_NRF52 && RAK_4631`.

`NRF52Bluetooth.cpp` allows the tested HOBO integrations to use one BLE peripheral link and one BLE central link:

```cpp
#if defined(SEEED_XIAO_NRF52840_KIT) || defined(RAK_4631)
    Bluefruit.configCentralBandwidth(BANDWIDTH_LOW);
    Bluefruit.begin(1, 1);
#else
    Bluefruit.begin();
#endif
```

This is required because Meshtastic itself uses BLE while the RAK simultaneously acts as the BLE central client for the HOBO logger.

## MX2001 identification

The production scanner does not lock to one hard-coded MAC.

It identifies the tested MX2001 advertisement by:

- manufacturer field length: 22 bytes
- first manufacturer bytes: `C5 00`

The discovered MAC is retained in human-readable order and included in every custom mesh payload.

## Onset GATT service

Service UUID:

```text
65e16e4f-ed4e-4641-ac49-83ccbce6cbcf
```

Primary command/notification characteristic:

```text
65e16f4f-ed4e-4641-ac49-83ccbce6cbcf
```

The MX2001 exposes additional proprietary characteristics, but they are not required for the current production path.

## Proven startup commands

### INIT

```text
01 01 04 05 1C 01 00
```

### Metadata block 0

```text
01 01 0A 0A 01 00 00 00 00 00 00 00 08
```

### Metadata block 8

```text
01 01 0A 0A 01 00 00 00 08 00 00 00 08
```

### STATUS

```text
01 01 08 04 05 00 00 00 00 00 00
```

The proven status response begins:

```text
01 02 04 05 ...
```

Values used by the production firmware:

- response bytes 8..11: current write pointer, big-endian
- response bytes 12..13: configured logging interval in seconds, big-endian

Example physically observed response:

```text
01 02 04 05 00 04 05 52 00 00 13 16 00 14 ...
```

which decoded to:

```text
write pointer = 0x00001316
interval      = 0x0014 = 20 seconds
```

This is why passive BLE advertisement timing is not used to infer the logging interval.

## NEWREAD64 direct sensor command

```text
01 01 08 04 04 00 00 00 00 00 00
```

The MX2001 returns two notification fragments for the direct read.

Representative response:

```text
01 02 04 04 00 04 04 ... 00 00 02 62 9E
02 05 12 ...
```

The current production path intentionally decodes only **temperature** and **WL**. AbsP, Baro, and DP are not needed for this project.

## Temperature decode

Direct temperature raw is in the first NEWREAD response:

```text
fragment 1 bytes 17..18, big-endian uint16
```

Conversion proven during the bench tests:

```text
temperature_F = -0.1805 * raw + 169.64
```

Examples:

```text
raw 610 -> about 59.54 F
raw 591 -> about 62.96 F
raw 495 -> about 80.29 F
```

The direct temperature repeatedly matched the logger's advertised temperature within the expected small timing/raw-count difference.

## Water-level decode

The MX2001's direct **WL** value is:

```text
fragment 2 bytes 3..6
```

interpreted as a **big-endian IEEE-754 float in meters**.

Conversion to feet:

```text
WL_ft = WL_m * 3.280839895
```

The decisive bench comparison was:

```text
advertised WL: -0.31952 ft
```

while the direct field was approximately:

```text
-0.0975 m
```

and:

```text
-0.0975 * 3.28084 ~= -0.320 ft
```

When the pressure sensor was placed near one foot underwater, the same direct field moved to about:

```text
0.257 to 0.271 m
```

or roughly:

```text
0.84 to 0.89 ft
```

This physically identified the field as the logger's water-level output rather than a guessed raw-pressure conversion.

The production firmware therefore uses the MX2001's already compensated/calibrated WL output and does not attempt to recreate AbsP/Baro/DP calculations.

## Write pointer and logger cadence

A separate memory/status test showed the write pointer advances when the logger creates a new record.

Examples:

```text
0x137B -> 0x1382   delta 7 bytes
0x1382 -> 0x138A   delta 8 bytes
```

The record size is therefore not assumed to be fixed.

The production sender uses this behavior as the authoritative trigger:

```text
read STATUS
    |
    +--> learn configured interval
    +--> establish current pointer

wait close to expected logger time
    |
    v
poll STATUS
    |
    +--> pointer unchanged: keep waiting
    |
    +--> pointer changed: a new logger record exists
                          |
                          v
                     NEWREAD64
                          |
                          +--> Temp
                          +--> WL
                          |
                          v
                     mesh packet
```

The configured interval is used to avoid unnecessary polling for most of the interval. The pointer change is still required before subsequent record-triggered transmissions.

## Startup behavior

After a successful connection and baseline STATUS, the stable firmware sends one immediate current snapshot:

```text
startup NEWREAD64 -> Temp + WL -> mesh TX
```

After that startup snapshot, new transmissions are triggered by actual write-pointer changes.

This provides an immediate health/current-value packet after a field-node restart without waiting an entire logger interval.

## Custom Meshtastic packet

Port:

```text
meshtastic_PortNum_PRIVATE_APP = 256
```

Payload length:

```text
19 bytes
```

Layout:

| Byte(s) | Meaning |
|---|---|
| 0 | ASCII `M` |
| 1 | ASCII `X` |
| 2 | version = `1` |
| 3 | flags; current value `0x03` means WL + temperature valid |
| 4..5 | sequence, uint16 little-endian |
| 6..7 | WL, signed int16 little-endian, tenths of a foot |
| 8..9 | temperature, signed int16 little-endian, tenths °F |
| 10..11 | raw MX2001 temperature uint16 little-endian |
| 12..17 | logger BLE MAC, human order |
| 18 | BLE RSSI, signed int8 |

The packet is broadcast on channel 0 and is routed/rebroadcast like any other Meshtastic packet on the configured mesh.

The normal Meshtastic phone UI does not know how to display this private payload. That is expected. The endpoint decoder interprets it.

## End-to-end production bench proof

A successful production run showed:

```text
MX2001 LOGGER READY
Interval: 20 seconds
Pointer: 0x000016A0
```

Startup direct read:

```text
WL: 0.919 ft
Temp: 80.29 F
Sequence: 1
```

First new logger record:

```text
Old pointer: 0x000016A0
New pointer: 0x000016A7
Delta: 7 bytes
WL: 0.913 ft
Temp: 80.29 F
Sequence: 2
```

Second new logger record:

```text
Old pointer: 0x000016A7
New pointer: 0x000016AE
Delta: 7 bytes
WL: 0.905 ft
Temp: 80.29 F
Sequence: 3
```

The firmware logged successful LoRa TX completion. Returned/rebroadcast copies of the same packet were also observed and correctly handled by Meshtastic packet history.

## Receiver proof

A second normal Meshtastic radio was connected to a Windows computer on COM5. The Python receiver decoded a later packet as:

```text
MX2001 DATA RECEIVED
Time:        2026-08-18T16:35:47-06:00
Mesh source: !b57d051f
Logger:      F1:0D:9D:29:C3:2D
Sequence:    29
Water level: 0.9 ft
Temperature: 80.5 F
BLE RSSI:    -65 dBm
LoRa RSSI:   -80 dBm
LoRa SNR:    6.5 dB
```

This is the end-to-end proof that the second radio received the data over LoRa and delivered the custom packet to the computer over USB.

## Receiver tool

The tested receiver is stored at:

```text
tools/mx2001_receiver.py
```

Dependencies:

```powershell
py -m pip install --upgrade meshtastic pypubsub
```

Run:

```powershell
py .\tools\mx2001_receiver.py --port COM5
```

The script:

- subscribes to `meshtastic.receive`
- filters for `PRIVATE_APP` / port 256
- checks for the `MX` payload marker
- decodes WL and temperature
- reports source node, logger MAC, BLE RSSI, LoRa RSSI, and LoRa SNR
- appends measurements to `mx2001_data.csv`

Do not run another serial monitor against the same receiver COM port while the Python receiver is running.

## Build and flash

Build:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e rak4631
```

Flash:

```powershell
$rak = (Get-Volume | Where-Object FileSystemLabel -eq "RAK4631").DriveLetter
Copy-Item ".pio\build\rak4631\firmware-rak4631-2.7.26.ded77c2.uf2" "$($rak):\"
```

Serial monitor for bench diagnostics:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor --port COM11 --baud 115200
```

COM ports vary by computer/device.

## Important rules for future changes

1. Preserve `mx2201-integration`; do not overwrite it with MX2001 work.
2. Treat `ada4c9526e68819506a44bf171aa5dff3de59660` as the first bench-proven MX2001 sender recovery commit.
3. Do not return to passive advertisement-cadence inference for the logger interval; STATUS already provides the real interval.
4. Do not reverse-engineer AbsP/Baro/DP unless a future requirement actually needs them; Temp and WL are already directly available.
5. Do not assume fixed memory-record length; observed pointer deltas include both 7 and 8 bytes.
6. Keep the custom packet versioned so future payload changes can remain backward-compatible.
7. After firmware changes, validate both the sender serial log and reception on a second physical Meshtastic radio.

## Next development stages

The core data path is complete. Remaining work is deployment hardening rather than MX2001 protocol discovery:

- long-duration creek test
- BLE reconnect/recovery stress test
- field power/solar validation
- receiver auto-reconnect service
- Raspberry Pi gateway
- database storage
- web dashboard
- alerting based on water-level thresholds/rate of rise
