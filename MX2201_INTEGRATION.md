# MX2201 Meshtastic Integration

This document records the bench-proven HOBO MX2201 integration used with a Seeed XIAO nRF52840 + Wio-SX1262 Meshtastic node.

Its purpose is to preserve the exact stable reference, protocol facts, design decisions, build target, recovery points, and physical bench-test evidence so future work does not repeat solved reverse engineering or accidentally regress the working system.

## Current stable reference

- Meshtastic base version: **2.7.26**
- Meshtastic base commit: `54e0d8d0ab2ff56b3a9ce967e53f79e49af560fb`
- Integration branch: `mx2201-integration`
- Current bench-proven firmware commit: `38891aa8e13708ce97de1d3bb4c493eafecb2886`
- Current stable tag: `mx2201-stable-newread-2026-08-13`
- Previous stable rollback tag: `mx2201-stable-2026-08-13`
- PlatformIO target: `seeed_xiao_nrf52840_kit`

The current stable tag is an immutable recovery point for the exact NEWREAD64 firmware that passed the final bench tests. Documentation-only commits may exist later on `mx2201-integration` without changing the tagged firmware.

Do not move either stable tag. If a future firmware version becomes stable, create a new tag.

## Hardware

### Sensor/logger

- Onset HOBO MX2201
- Logger serial used during protocol discovery: `21680233`
- BLE MAC used by this implementation: `EB:9A:E4:52:6D:5F`
- Bluetooth Always On: enabled during the final unattended-reconnect tests

### Meshtastic sensor node

- Seeed XIAO nRF52840
- Seeed Wio-SX1262 radio board
- Meshtastic displayed name used during testing: **DWQ Data Node**
- Node ID used during testing: `!c22d7fec`
- Project nickname for this XIAO + Wio-SX1262 hardware format: **Chew can node**

## Proven end-to-end chain

The full path below was physically bench-tested:

```text
HOBO MX2201
    |
    | BLE central connection
    v
Seeed XIAO nRF52840 + Wio-SX1262
    |
    | standard Meshtastic EnvironmentMetrics.temperature
    | over LoRa
    v
Second Meshtastic node
    |
    | BLE
    v
Android Meshtastic app
```

The receiving Meshtastic app displayed the same temperature values produced by the source node's direct MX2201 read.

## Files intentionally changed from stock Meshtastic 2.7.26

The MX2201 firmware integration is intentionally narrow. The source changes are confined to:

```text
src/modules/Modules.cpp
src/modules/Telemetry/MX2201Telemetry.cpp
src/modules/Telemetry/MX2201Telemetry.h
src/platform/nrf52/NRF52Bluetooth.cpp
```

`MX2201Telemetry.cpp` and `MX2201Telemetry.h` are custom integration files. `Modules.cpp` and `NRF52Bluetooth.cpp` contain the minimum registration and dual-BLE changes required for the tested XIAO target.

## BLE dual-role configuration

The XIAO must remain available to the Meshtastic phone app while simultaneously acting as a BLE central client to the MX2201.

The proven nRF52 configuration is:

```cpp
Bluefruit.autoConnLed(false);
Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
#if defined(SEEED_XIAO_NRF52840_KIT)
    Bluefruit.configCentralBandwidth(BANDWIDTH_LOW);
    Bluefruit.begin(1, 1);
#else
    Bluefruit.begin();
#endif
```

This provides:

- one BLE peripheral connection for the Meshtastic phone
- one BLE central connection for the HOBO MX2201

Do not replace this arrangement without direct runtime evidence that it is necessary.

## MX2201 BLE identity

### Service UUID

```text
65e16e4f-ed4e-4641-ac49-83ccbce6cbcf
```

### Command / notification characteristic UUID

```text
65e16f4f-ed4e-4641-ac49-83ccbce6cbcf
```

## Proven startup command sequence

### Initialize logger

```text
01 01 04 05 1C 01 00
```

### Read metadata block 0

```text
01 01 0A 0A 01 00 00 00 00 00 00 08 00
```

### Read metadata block 8

```text
01 01 0A 0A 01 00 00 08 00 00 00 08 00
```

### Read status / write pointer

This command is exactly 11 bytes:

```text
01 01 08 04 05 00 00 00 00 00 00
```

The status response provides at least the values used by this firmware:

- current write pointer
- current MX2201 logging interval

The logging interval is read dynamically. It is not hard-coded.

## NEWREAD64 live-sensor command

The major production change in the current stable firmware is direct live-sensor acquisition using Onset's `NEWREAD64` command.

Command:

```text
01 01 08 04 04 00 00 00 00 00 00
```

This differs from the status command only by the subcommand byte:

```text
STATUS:  01 01 08 04 05 00 00 00 00 00 00
NEWREAD: 01 01 08 04 04 00 00 00 00 00 00
```

The command was identified from Onset's application/SDK behavior and then verified directly against the physical MX2201.

## Proven NEWREAD64 response layout

The production parser requires this exact response prefix:

```text
01 01 07 04 04 00 04 04
```

The live temperature raw value is the 4-byte big-endian integer in bytes 8 through 11:

```text
01 01 07 04 04 00 04 04 [TEMP32 BE] ...
```

Verified examples include:

```text
00 00 06 CD -> raw 1741 -> about 82.11 F
00 00 06 D8 -> raw 1752 -> about 82.96 F
00 00 06 DF -> raw 1759 -> about 83.50 F
```

Another final stable bench run produced:

```text
raw 1478 -> 61.81 F / 16.56 C
```

Do not infer battery-byte meaning from the remaining response bytes unless independently proven.

## Temperature calibration

The already-proven calibration was preserved unchanged when NEWREAD64 became the active acquisition path.

```cpp
RAW_TO_F_SLOPE = 0.0771942720f
RAW_TO_F_INTERCEPT = -52.2825573f
```

Conversion:

```text
temperature_F = 0.0771942720 * raw - 52.2825573
temperature_C = (temperature_F - 32) * 5 / 9
```

Equivalent Onset form:

```text
temperature_C = raw * 175.72 / 4096 - 46.85
```

Plausible raw values are constrained to:

```text
400 <= raw <= 2400
```

## Why NEWREAD64 replaced historical memory as the active path

The original integration successfully decoded temperature from the logger's historical memory. That work proved the BLE framing, packed 12-bit data format, moving phase alignment, calibration, and continuity rules.

However, at long logger intervals, a 64-byte historical-memory window can contain stale or non-temperature patterns that accidentally satisfy the old smooth-sequence tests. This became obvious during one-hour logging tests, where the memory decoder could select a plausible-looking but incorrect historical candidate even though the direct live-sensor response was correct.

The production architecture therefore changed to:

```text
STATUS -> determine logger interval and maintain connection
NEWREAD64 -> obtain current live temperature
Meshtastic telemetry -> transmit that fresh live value
```

The old memory decoder remains in the source as legacy/proven historical work, but it is not the active live-temperature acquisition path in the current stable state.

Do not reconnect the memory decoder to normal live telemetry unless there is a specific new requirement and it is separately bench-tested.

## Historical memory decoder facts

These facts are retained because they were physically proven and may be useful for future archival-data work.

### Memory request

```text
01 01 0A 0A 01 [4-byte address big-endian] [4-byte length big-endian]
```

For a 64-byte request, the length is:

```text
00 00 00 40
```

### 64-byte notification reconstruction

```text
fragment 0x01 -> skip 5 bytes, copy 15 bytes
fragment 0x02 -> skip 1 byte, copy 19 bytes
fragment 0x03 -> skip 1 byte, copy 19 bytes
fragment 0x04 -> skip 1 byte, copy 11 bytes
```

Total:

```text
15 + 19 + 19 + 11 = 64 bytes
```

### Packed 12-bit phases

The historical memory stream can begin on any of three nibble phases:

```text
phase 0
phase 1
phase 2
```

The proven legacy decoder used recency-first candidate selection, `MAX_RAW_STEP=100`, `MAX_ACCEPTED_RAW_JUMP=250`, control-record filtering, and confirmation of large real temperature changes. Those rules solved real historical-memory decoding problems, but they are no longer required for normal live telemetry because NEWREAD64 returns the direct live temperature value.

## Current telemetry architecture

The production state machine intentionally separates three concepts that were previously coupled.

### 1. BLE/status keepalive

Status is polled at most every 10 seconds, even when the MX2201 logging interval is much longer.

Purpose:

- keep the BLE protocol active
- know the current logger interval
- retain the write pointer as a diagnostic/health signal

A 10-second status poll is **not** a 10-second temperature measurement.

### 2. Write pointer

Write-pointer changes are diagnostic only.

A pointer change does **not** trigger:

- a memory read
- a NEWREAD64 request
- a Meshtastic temperature transmission

This was specifically bench-tested after pressing the logger's center button. The pointer changed, the firmware logged the change with `(no telemetry trigger)`, and no extra MX2201 temperature packet was created.

The center button must not be described as a manual temperature-reading command; its exact relationship to the observed pointer movement was not established.

### 3. Temperature reporting

Immediately after a successful startup/reconnect sequence, the node requests one fresh NEWREAD64 value and transmits it once.

For scheduled reports, the firmware does **not** resend the cached temperature. Instead it requests NEWREAD64 first and transmits the newly acquired live value.

## Reporting interval

The MX2201 logger's own interval drives the field temperature-reporting interval.

For logger intervals of 60 seconds or longer:

```text
MX2201 interval 60 s    -> mesh temperature about every 60 s
MX2201 interval 600 s   -> mesh temperature about every 10 min
MX2201 interval 1800 s  -> mesh temperature about every 30 min
MX2201 interval 3600 s  -> mesh temperature about every 1 hour
```

Short bench logging intervals are clamped to a minimum 60-second mesh-report interval so a 1-second or 10-second bench setting cannot flood LoRa.

If the logger interval has not yet been obtained from status, the code retains a 60-second fallback.

## One-hour logger test

The current architecture was specifically tested with the MX2201 set to:

```text
3600 seconds
```

Observed startup sequence:

```text
MX2201 STATUS: ... interval=3600 seconds
MX2201 TX: READ LIVE SENSORS (NEWREAD64)
MX2201 NEWREAD64 LIVE SENSOR
...
MX2201: SENDING STANDARD MESHTASTIC TELEMETRY
```

The node then continued status polling at approximately 10-second intervals without retransmitting the cached temperature every minute.

A later write-pointer change was logged as:

```text
logger pointer changed ... (no telemetry trigger)
```

and no additional MX2201 temperature transmission occurred because of that pointer change.

This is the key proof that the logger's one-hour internal schedule and Meshtastic live-temperature reporting are no longer accidentally coupled to pointer movement or a one-minute cached-value timer.

## BLE discovery and unattended restart behavior

The final firmware continuously participates in BLE scanning until the target MX2201 is found.

A bench reboot/flash was performed with the MX2201 already configured with Bluetooth Always On. No center-button press was used.

The observed sequence was:

```text
MX2201: starting continuous scan for EB:9A:E4:52:6D:5F
MX2201: target logger found
MX2201: connecting
[connection establishment delay]
MX2201: BLE connection established
```

The node then completed service discovery, characteristic discovery, status, NEWREAD64, and telemetry automatically.

This proves the tested system can recover from a sensor-node restart without a person physically waking the logger.

## Asynchronous BLE connection handling

`Bluefruit.Central.connect(report)` is asynchronous. During testing there could be several seconds between:

```text
target logger found
connecting
```

and:

```text
BLE connection established
```

Without an explicit pending-connection state, `runOnce()` continued trying to start new scans during that delay and produced repeated scan-start warnings.

The final firmware uses `connectionInProgress` so the module waits quietly for the connect callback instead of starting additional scans.

Final bench logs showed a clean sequence with no repeated scan warnings during the pending connection interval.

## Standard Meshtastic telemetry

The integration uses standard Meshtastic telemetry, not a proprietary packet.

The transmitted protobuf is:

```text
TELEMETRY_APP
  -> EnvironmentMetrics
     -> temperature
```

Equivalent behavior:

```cpp
meshtastic_Telemetry telemetry = meshtastic_Telemetry_init_zero;
telemetry.time = getTime();
telemetry.which_variant = meshtastic_Telemetry_environment_metrics_tag;
telemetry.variant.environment_metrics.has_temperature = true;
telemetry.variant.environment_metrics.temperature = temperatureC;
```

The reading is:

- added to the local node database
- sent over the LoRa mesh
- available to the connected phone using the same standard Meshtastic telemetry protobuf

## Duplicate packet behavior

Two different issues were separated during bench testing.

### Old immediate startup duplicate

An older version could send the first valid telemetry twice due to timestamp underflow inside the same `runOnce()` execution. That was fixed before the NEWREAD64 production work.

### Apparent duplicate returned through the mesh

A later test showed the source node creating one telemetry packet and then receiving the same packet back through a relay. Meshtastic's packet history recognized the returned packet and filtered it as a duplicate.

Therefore, a repeated display in the app does not automatically prove the source firmware created a second unique telemetry packet. Serial packet IDs and packet-history logs must be used to distinguish a true source retransmission from a relayed duplicate.

## Module registration

`MX2201TelemetryModule` is instantiated only for:

```cpp
#if defined(ARCH_NRF52) && defined(SEEED_XIAO_NRF52840_KIT)
```

This intentionally scopes the custom integration to the tested XIAO target.

## Build procedure

From the repository root on Windows:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e seeed_xiao_nrf52840_kit
```

Do not substitute `xiao_ble_33db` for the proven target.

Preferred flash procedure:

1. Build the UF2.
2. Double-tap the XIAO reset button.
3. Wait for the bootloader drive.
4. Copy the generated UF2 to the bootloader drive.
5. Allow the board to reboot.

Do not erase the device as part of normal updating or debugging unless there is a separate proven reason to do so.

## Serial monitor

Typical Windows command:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" -m serial.tools.miniterm COM7 115200
```

Exit with:

```text
Ctrl + ]
```

The COM port may differ on another machine.

## Important development rules

1. **Keep Meshtastic pinned to 2.7.26 for this stable integration unless an upgrade is a separate deliberate project.**
2. **Do not erase the board as a first-line debugging step.**
3. **Do not broadly rewrite the MX2201 module for a small issue.**
4. **Make the smallest possible source change.**
5. **Fix one issue at a time and bench-test it before the next change.**
6. **Preserve known-good commits and create new stable tags rather than moving old tags.**
7. **Do not alter the proven temperature calibration without independent physical evidence.**
8. **Do not redo solved BLE UUID, command, framing, calibration, or dual-role work.**
9. **Use standard Meshtastic telemetry rather than inventing a custom packet format.**
10. **Keep the build target `seeed_xiao_nrf52840_kit`.**
11. **Do not use write-pointer changes as the live-temperature trigger.**
12. **Do not retransmit cached temperature on a separate one-minute timer.**
13. **Obtain a fresh NEWREAD64 value before each scheduled temperature transmission.**
14. **Treat `mx2201-stable-newread-2026-08-13` as the current immutable production recovery point.**
15. **Treat `mx2201-stable-2026-08-13` as the preserved rollback point for the previous memory-decoder stable version.**
16. **For another logger model such as MX2001, use a separate branch and do not disturb the stable MX2201 path.**

## Key commit history

```text
54e0d8d0a  Meshtastic 2.7.26 pinned base
cbe6b11ac  Enable BLE central connection for MX2201
9f257ac17  Add MX2201 BLE connection test module
63ac7ff85  Read MX2201 temperature and send Meshtastic telemetry
58a7df80c  Broad experimental fix; intentionally superseded later
f7c937e76  Restore proven MX2201 integration
d79862d98  Fix MX2201 memory fragment reconstruction
2cb9a5400  Fix MX2201 latest-sample continuity
3aa59427b  Keep MX2201 BLE active with slow logging
6302d7bbc  Fix MX2201 temperature phase selection
f68aad7e2  Prevent duplicate initial MX2201 telemetry
38891aa8e  Use MX2201 NEWREAD64 for interval-aligned telemetry
```

The history is intentionally preserved. Do not rewrite it simply because experimental commits exist in the ancestry.

## What has been physically proven in the current stable version

- MX2201 BLE discovery without pressing the logger button after sensor-node reboot
- asynchronous central connection handling
- HOBO service discovery
- command characteristic discovery
- notification enablement
- status/write-pointer parsing
- dynamic logger interval parsing
- direct NEWREAD64 request and response
- exact NEWREAD64 temperature field parsing from bytes 8..11
- correct temperature calibration
- direct live temperature at a 3600-second logger interval without waiting one hour for a new logged sample
- one startup telemetry transmission from the fresh live read
- 10-second status keepalive while the logger is configured for one-hour logging
- no cached one-minute retransmission during one-hour logging
- write-pointer changes producing no extra temperature telemetry
- standard Meshtastic environmental telemetry creation
- LoRa transmission
- reception by another Meshtastic node
- Android display of the received temperature
- clean pending-connection handling without repeated scan warnings

## What the older stable version additionally proved

The preserved legacy tag `mx2201-stable-2026-08-13` contains the memory-decoder implementation that physically proved:

- 64-byte memory reconstruction from four BLE fragments
- packed 12-bit temperature decoding
- moving phase alignment
- recency-first phase selection
- false-candidate rejection
- large cold-to-hot transition handling
- large hot-to-cold transition handling

Those results remain valuable protocol evidence, even though historical memory is no longer the active production live-temperature source.

## What is not implied

This integration does not prove that:

- every MX2201 has the same BLE MAC
- every MX2201 firmware revision behaves identically
- MX2001 uses the same command mapping or response format
- future Meshtastic releases can accept these changes without adaptation
- other nRF52 boards will behave identically

## Recovery instructions

### Current stable NEWREAD64 firmware

```powershell
git fetch origin --tags
git checkout mx2201-stable-newread-2026-08-13
```

This intentionally leaves Git in detached HEAD state because the stable tag is immutable.

### Previous stable memory-decoder firmware

```powershell
git checkout mx2201-stable-2026-08-13
```

### Return to normal integration development

```powershell
git checkout mx2201-integration
git pull origin mx2201-integration
```

Before development for another logger model, create a separate branch rather than modifying either stable tag or the proven MX2201 history.
