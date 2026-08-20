# Shared HOBO Protocol

This folder documents the universal HOBO logic shared by both production radio targets:

- Seeed XIAO nRF52840 + Wio-SX1262
- RAK4631 / RAK19003

## Production branches

Current production:

```text
hobo-mx2001-mx2201-mx2203
```

Frozen validated snapshot:

```text
hobo-universal-validated-2026-08-19
```

## Supported loggers

| Logger | Automatic telemetry | Direct `READ` |
|---|---|---|
| MX2001 | Water level + temperature | Water level + temperature |
| MX2201 | Temperature | Temperature |
| MX2203 | Temperature | Temperature |

## Automatic telemetry timing

The production firmware **does automatically transmit new HOBO records**.

It does not use a blind periodic radio timer. Instead, it synchronizes to the logger itself:

1. Connect to the HOBO over BLE.
2. Perform an identification/probe `NEWREAD64`; this startup probe is not automatically transmitted.
3. Request HOBO `STATUS`.
4. Read the logger's configured logging interval and current write pointer.
5. Poll until the write pointer advances, establishing the real logger record boundary.
6. Perform one fresh `NEWREAD64`.
7. Queue one Meshtastic packet.
8. Mark that write pointer transmitted only after successful queueing.
9. Repeat at each subsequent HOBO record boundary.

This means the radio follows whatever interval is configured in the physical HOBO: 20 seconds, 300 seconds, 600 seconds, etc.

A direct manual `READ` is independent of this automatic schedule and does not consume the pending automatic pointer.

If repeated `STATUS` requests fail, automatic telemetry pauses and retries `STATUS`; it does not revert to guessed periodic transmissions.

### Final interval validation

On physical RAK4631 hardware, MX2201 logger `E4:27:8C:B9:F4:B8` was configured for a 20-second interval. Consecutive automatic packet timing was observed as:

```text
count=1  pointer_to_tx=202 ms  cadence=0 ms
count=2  pointer_to_tx=202 ms  cadence=19848 ms
count=3  pointer_to_tx=202 ms  cadence=19879 ms
```

Average non-baseline cadence: **19.864 seconds**.

Each packet followed a confirmed HOBO write-pointer change and fresh `NEWREAD64` read.

## Meshtastic commands

Commands are sent as direct text messages to the field radio. A leading `/` is optional and matching is case-insensitive.

### `LOGGER`

Reports the connected logger without forcing a measurement:

```text
HOBO CONNECTED
Model: MX2201
MAC: E4:27:8C:B9:F4:B8
BLE: -67 dBm
Interval: 20 sec
Lock: OFF
```

When locked it also reports the saved target MAC. If the target is absent, it reports that it is waiting for the locked target.

### `READ`

Performs one fresh `NEWREAD64` measurement and replies directly to the requester. The reply includes model, full BLE MAC, BLE RSSI and the current measurement.

### `LOCK`

Persistently assigns the radio to the currently connected, positively identified MX2001/MX2201/MX2203. The logger BLE address is saved in:

```text
/prefs/hobo_lock.bin
```

After reboot, the radio restores that target and ignores every other HOBO until the assignment is cleared.

### `UNLOCK`

Deletes the saved logger assignment, releases the current BLE link and resumes discovery of any supported HOBO.

## Field workflow

1. Flash the universal firmware.
2. Keep the radio unlocked during bench work.
3. At the actual monitoring site, place it beside the intended HOBO.
4. Send `LOGGER` and verify model, MAC and logging interval.
5. Send `LOCK`.
6. Reboot once and verify with `LOGGER` before leaving the site.

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

STATUS:

```text
01 01 08 04 05 00 00 00 00 00 00
```

The STATUS response supplies the write pointer and logger interval used for automatic scheduling.

## Model response identification

MX2201:

```text
01 01 07 04 04 00 04 04 [TEMP32 BE] ...
```

MX2203:

```text
01 01 0B 04 04 00 04 04 [TEMP32 BE] ...
```

MX2001 uses the hardware-proven two-fragment live response.

## Decoding

### MX2203

```text
C = raw × 175.72 / 16384 - 46.85
F = C × 9/5 + 32
```

Recovered from `OnsetSDK.dll` and hardware validated.

### MX2201

```text
F = 0.0771942720 × raw - 52.2825573
C = (F - 32) × 5/9
```

### MX2001

```text
Temperature F = -0.1805 × raw + 169.64
Level feet = big-endian stage float in meters × 3.280839895
```

## Discovery and assignment behavior

No production logger MAC is hard-coded in the firmware.

When unlocked, the node discovers compatible HOBOs dynamically and maintains one logger BLE connection at a time. If several are nearby, discovery order determines which one it initially selects.

When `LOCK` is active, only the saved BLE MAC is eligible for connection. If that logger is unavailable, the radio waits for it instead of silently switching to another site/logger.

## Source files

```text
src/modules/Telemetry/HOBOMX2001MX2201MX2203/
├── HOBOMX2001MX2201MX2203Telemetry.cpp
├── HOBOMX2001MX2201MX2203Telemetry.h
├── HOBOMX2001MX2201MX2203TelemetryRAK.cpp
├── ONSETSDK.md
└── README.md
```

Shared BLE setup:

```text
src/platform/nrf52/NRF52Bluetooth.cpp
```

Module registration:

```text
src/modules/Modules.cpp
```

Permanent APK/OnsetSDK reverse-engineering details remain in `ONSETSDK.md`.