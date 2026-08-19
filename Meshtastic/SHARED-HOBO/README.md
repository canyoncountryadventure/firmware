# Shared HOBO Protocol

This folder documents the common HOBO logic used by both production radio targets:

- Seeed XIAO nRF52840 + Wio-SX1262
- RAK4631 / RAK19003

Both use the same HOBO BLE protocol implementation. The radio-specific code only handles board integration/compilation.

## Production branch

```text
hobo-mx2001-mx2201-mx2203
```

## Supported loggers

| Logger | Data returned by direct `READ` |
|---|---|
| MX2001 | Water level + temperature |
| MX2201 | Temperature |
| MX2203 | Temperature |

## Meshtastic command

Send a direct text message to the field node:

```text
READ
```

`/READ`, lowercase, and mixed case are accepted.

The firmware does not periodically poll the HOBO in this production build. It keeps/establishes a BLE connection, waits for a direct `READ`, performs a fresh `NEWREAD64` measurement, and replies over Meshtastic.

## BLE protocol

HOBO service UUID:

```text
CFCBE6BC-CC83-49AC-4146-4EED4F6EE165
```

Command/notification characteristic:

```text
CFCBE6BC-CC83-49AC-4146-4EED4F6FE165
```

INIT:

```text
01 01 04 05 1C 01 00
```

NEWREAD64:

```text
01 01 08 04 04 00 00 00 00 00 00
```

## Model response identification

MX2201:

```text
01 01 07 04 04 00 04 04 [TEMP32 BE] ...
```

MX2203:

```text
01 01 0B 04 04 00 04 04 [TEMP32 BE] ...
```

MX2001 uses the known two-fragment live response recovered and physically validated during the MX2001 work.

## Temperature / level decoding

### MX2203

The production code uses the conversion recovered from `OnsetSDK.dll` in the HOBOconnect Android APK:

```text
C = raw × 175.72 / 16384 - 46.85
F = C × 9/5 + 32
```

This was validated against the physical MX2203 data export.

### MX2201

The current production universal code intentionally preserves the already hardware-proven calibration used by the combined MX2201/MX2001 reader:

```text
F = 0.0771942720 × raw - 52.2825573
C = (F - 32) × 5/9
```

The exact HOBOconnect `TempSensor32` formula is also known from the APK reverse engineering, but the production universal code was not changed after successful hardware validation solely for cosmetic/precision cleanup.

### MX2001

Current production decoding preserves the hardware-proven MX2001 path:

```text
Temperature F = -0.1805 × raw + 169.64
Level feet = big-endian stage float in meters × 3.280839895
```

## Discovery behavior

No logger MAC is hard-coded in the production universal firmware.

The node scans for Onset/HOBO candidates and connects to one valid logger at a time. If multiple HOBOs are simultaneously in range, it may connect to whichever suitable candidate it reaches first.

For the intended field deployment this is acceptable because one monitoring site is expected to have one nearby HOBO logger.

## Source files

Primary implementation:

```text
src/modules/Telemetry/HOBOMX2001MX2201MX2203/
├── HOBOMX2001MX2201MX2203Telemetry.cpp
├── HOBOMX2001MX2201MX2203Telemetry.h
├── HOBOMX2001MX2201MX2203TelemetryRAK.cpp
├── ONSETSDK.md
└── README.md
```

Shared Bluetooth setup:

```text
src/platform/nrf52/NRF52Bluetooth.cpp
```

Module registration:

```text
src/modules/Modules.cpp
```

The Bluetooth layer reserves one BLE peripheral connection for the Meshtastic phone and one BLE central connection for the HOBO logger on both supported nRF52840 targets.

## APK reverse-engineering record

Permanent details from the HOBOconnect / OnsetSDK reverse engineering are kept in:

```text
src/modules/Telemetry/HOBOMX2001MX2201MX2203/ONSETSDK.md
```

Do not replace those recovered formulas or protocol facts with guesses from unsynchronized field samples.
