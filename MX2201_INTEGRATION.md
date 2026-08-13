# MX2201 Meshtastic Integration

This document records the bench-proven HOBO MX2201 integration used with a Seeed XIAO nRF52840 + Wio-SX1262 Meshtastic node.

The purpose of this file is to preserve not only the code state, but also the protocol facts, design decisions, known-good build target, and bench-test evidence so future development does not repeat solved work.

## Stable reference

- Meshtastic base version: **2.7.26**
- Base commit: `54e0d8d0ab2ff56b3a9ce967e53f79e49af560fb`
- Working branch: `mx2201-integration`
- Bench-proven firmware commit: `f68aad7e283bd5cfabfeca3f4b7544345860d32c`
- Stable tag: `mx2201-stable-2026-08-13`
- PlatformIO target: `seeed_xiao_nrf52840_kit`

Do not substitute `xiao_ble_33db` for the build target. The canonical target above was runtime-proven with stable Android BLE and the MX2201 central connection.

The stable tag points to the exact firmware that passed the bench tests. Later documentation-only commits may exist on the branch, but the tag remains the immutable recovery point for the tested code.

## Hardware

### Sensor/logger

- Onset HOBO MX2201
- Logger serial used during protocol discovery: `21680233`
- BLE MAC used by this implementation: `EB:9A:E4:52:6D:5F`

### Meshtastic node

- Seeed XIAO nRF52840
- Seeed Wio-SX1262 radio board
- Project nickname: **Chew can node**

## Proven end-to-end chain

The following full path was bench-proven:

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

The receiving node and Android app were observed following the same temperature values shown in the source node serial log during the hot/cold bench tests.

## Files intentionally changed from stock Meshtastic 2.7.26

Only these four source files should differ from the pinned base for the MX2201 integration:

```text
src/modules/Modules.cpp
src/modules/Telemetry/MX2201Telemetry.cpp
src/modules/Telemetry/MX2201Telemetry.h
src/platform/nrf52/NRF52Bluetooth.cpp
```

`MX2201Telemetry.cpp` and `MX2201Telemetry.h` are new files. `Modules.cpp` and `NRF52Bluetooth.cpp` are stock Meshtastic files with narrow integration changes.

## BLE dual-role configuration

The XIAO must remain usable by the Meshtastic phone app while simultaneously acting as a BLE central client to the MX2201.

The proven modification in `src/platform/nrf52/NRF52Bluetooth.cpp` is:

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

- one BLE peripheral connection for the phone
- one BLE central connection for the HOBO MX2201

This exact arrangement was runtime-proven. Do not replace it without direct evidence that it is broken.

## MX2201 BLE protocol facts

### Service UUID

```text
65e16e4f-ed4e-4641-ac49-83ccbce6cbcf
```

### Command / notification characteristic UUID

```text
65e16f4f-ed4e-4641-ac49-83ccbce6cbcf
```

### Initialization command

```text
01 01 04 05 1C 01 00
```

### Metadata block 0

```text
01 01 0A 0A 01 00 00 00 00 00 00 08 00
```

### Metadata block 8

```text
01 01 0A 0A 01 00 00 08 00 00 00 08 00
```

### Status / write-pointer request

This command is exactly 11 bytes:

```text
01 01 08 04 05 00 00 00 00 00 00
```

The status response provides:

- current write pointer
- logger logging interval

The logger interval is dynamic and must be read from status. Do not hard-code the logger measurement interval.

## Memory read command

The 13-byte memory request is:

```text
01 01 0A 0A 01 [4-byte address big-endian] [4-byte length big-endian]
```

The integration reads the 64 bytes immediately preceding the current write pointer.

For a 64-byte read, the length field is:

```text
00 00 00 40
```

## Proven memory notification framing

The requested 64 bytes arrive across four BLE notifications.

The proven reconstruction is:

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

This framing was discovered from runtime notification data and then bench-proven. Do not revert to the earlier `0x0B` framing assumption.

## Temperature encoding

MX2201 temperature data is packed as 12-bit values. The 64-byte memory window can begin at any nibble alignment, so the decoder evaluates three possible phases:

```text
phase 0
phase 1
phase 2
```

The correct phase can move between successive 64-byte windows as the write pointer advances. Do not assume a fixed phase.

## Calibration

The calibration below was previously derived and confirmed. It must not be changed unless independent MX2201 measurements prove it wrong.

```cpp
RAW_TO_F_SLOPE = 0.0771942720f
RAW_TO_F_INTERCEPT = -52.2825573f
```

Conversion:

```text
temperature_F = 0.0771942720 * raw - 52.2825573
temperature_C = (temperature_F - 32) * 5 / 9
```

Known example:

```text
raw 1242 -> approximately 43.59 F / 6.44 C
```

## Decoder rules

### Plausible raw range

```text
400 <= raw <= 2400
```

### Smoothness inside a candidate sequence

```text
MAX_RAW_STEP = 100
```

This is used to identify a smooth sequence within one candidate phase.

### Cross-window continuity threshold

```text
MAX_ACCEPTED_RAW_JUMP = 250
```

A candidate more than 250 raw counts away from the last accepted value is treated as suspicious unless the newest data strongly confirms a real rapid temperature change.

### Large real temperature changes

A large jump is accepted only when:

```text
candidate.recency == 0
AND
candidate.stableCount >= 3
```

This rule was added because real rapid changes can legitimately exceed the 250-raw continuity threshold.

It was physically bench-tested in both directions.

#### Cold to hot test

The sensor was moved from cold conditions to approximately 102 F water.

Observed behavior included:

- newest large-jump candidate with `stable=2` was held
- next newest sequence with `stable=3` was accepted as a confirmed large change
- subsequent values tracked upward through approximately 64.7 F, 91.6 F, 94.3 F, and 95.9 F while continuing toward equilibrium

This proved that a real large upward transition does not remain stuck on stale cold history.

#### Hot to ice test

Starting near 95.9 F, the sensor was placed directly into ice water.

Observed behavior included:

- newest cold candidate `raw=1427`, `stable=2`, `recency=0` was held by continuity
- on the next confirmed sequence, `raw=1365`, `stable=3`, `recency=0` was accepted as a confirmed large temperature change
- accepted values then continued downward through approximately 53.1 F, 50.0 F, and 48.0 F

This proved the same rule works in the downward direction.

## Recency is the primary phase discriminator

A prior bug allowed a long, smooth but older sequence deeper in the 64-byte window to outscore a newer real sequence.

The current rule is:

1. Prefer the valid candidate with the lowest `recency`.
2. Use score only as a tie-breaker when recency is equal.

This prevents stale historical data from dominating newly logged temperature data.

Do not revert to score-first phase selection.

## Control-record filtering

Correctly aligned MX2201 control records can decode as:

```text
FFF E00 xxx
```

The decoder skips these records during temperature sequence scoring.

They are not used to determine the phase.

## Slow logger intervals and BLE keepalive

A 60-second MX2201 logger interval originally caused the BLE connection to drop after approximately one minute of protocol inactivity.

The fix does **not** change the MX2201 logging interval.

Instead, the firmware checks the write pointer at most every 10 seconds:

```text
logger measures every 60 seconds
firmware asks for status every 10 seconds
memory is read only when the write pointer changes
```

This keeps the BLE protocol active while preserving the logger's real measurement schedule.

The 60-second logger remained connected for extended bench runs after this change.

Do not interpret the 10-second status poll as a 10-second measurement interval.

## Telemetry behavior

The integration uses standard Meshtastic telemetry, not a proprietary packet.

The transmitted protobuf is:

```text
TELEMETRY_APP
  -> EnvironmentMetrics
     -> temperature
```

Equivalent code behavior:

```cpp
meshtastic_Telemetry telemetry = meshtastic_Telemetry_init_zero;
telemetry.time = getTime();
telemetry.which_variant = meshtastic_Telemetry_environment_metrics_tag;
telemetry.variant.environment_metrics.has_temperature = true;
telemetry.variant.environment_metrics.temperature = temperatureC;
```

The telemetry is:

- added to the local node database
- transmitted over the LoRa mesh
- made available to the connected phone using the same standard Meshtastic telemetry protobuf

## Telemetry transmission interval

The Meshtastic telemetry transmission interval is independent of the MX2201 logging interval.

If Meshtastic environmental telemetry has no configured update interval, the integration uses a 60-second fallback transmission interval.

## Duplicate first-transmission bug

A startup bug caused the first valid temperature to be transmitted twice immediately.

Cause:

- `runOnce()` captured `now = millis()`
- first telemetry send stored `lastTelemetrySentMs = millis()` a few milliseconds later
- later in the same `runOnce()`, unsigned subtraction `now - lastTelemetrySentMs` underflowed
- the periodic-send check therefore appeared to have already expired

The fix is to record the first successful send using the existing `now` value:

```cpp
lastTelemetrySentMs = now;
```

Bench logs after the fix showed transmissions spaced at approximately the intended 60-second interval with no immediate duplicate first packet.

## Module registration

`MX2201TelemetryModule` is instantiated only for:

```cpp
#if defined(ARCH_NRF52) && defined(SEEED_XIAO_NRF52840_KIT)
```

This keeps the custom integration scoped to the intended XIAO nRF52840 build target.

## Build procedure

From the repository root on Windows:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e seeed_xiao_nrf52840_kit
```

Preferred flash method:

1. Build the UF2.
2. Double-tap the XIAO reset button.
3. Wait for the bootloader drive to appear.
4. Copy the generated UF2 to the bootloader drive.
5. Allow the board to reboot.

Manual UF2 flashing is preferred over changing the working upload flow during debugging.

## Serial monitor

Typical Windows command:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" -m serial.tools.miniterm COM7 115200
```

Exit miniterm with:

```text
Ctrl + ]
```

The COM port may differ on another machine.

## Important development rules

Future work should follow these rules unless direct evidence requires otherwise:

1. **Do not upgrade Meshtastic as part of MX2201 debugging.**
2. **Do not erase the board as a first-line debugging step.**
3. **Do not rewrite the entire MX2201 module for a small bug.**
4. **Make the smallest possible source change.**
5. **Fix one issue at a time.**
6. **Preserve known-good commits before further changes.**
7. **Do not alter the proven calibration without independent evidence.**
8. **Do not redo the solved UUID, command, memory-framing, phase, or BLE-central discovery work.**
9. **Use standard Meshtastic telemetry rather than inventing a custom packet format.**
10. **Keep the build target `seeed_xiao_nrf52840_kit`.**
11. **Treat `mx2201-stable-2026-08-13` as the immutable recovery point for the bench-proven firmware.**

## Commit history worth understanding

Key commits in the development path include:

```text
54e0d8d0a  Meshtastic 2.7.26 pinned base
cbe6b11ac  Enable BLE central connection for MX2201
9f257ac17  Add MX2201 BLE connection test module
63ac7ff85  Read MX2201 temperature and send Meshtastic telemetry
58a7df80c  Broad experimental fix; later intentionally superseded
f7c937e76  Restore proven MX2201 integration
d79862d98  Fix MX2201 memory fragment reconstruction
2cb9a5400  Fix MX2201 latest-sample continuity
3aa59427b  Keep MX2201 BLE active with slow logging
6302d7bbc  Fix MX2201 temperature phase selection
f68aad7e2  Prevent duplicate initial MX2201 telemetry
```

The existence of the experimental `58a7df80c` commit is not a reason to rewrite history. The later recovery and surgical commits document how the stable implementation was restored and fixed.

## What has been physically proven

The stable implementation has been bench-tested for all of the following:

- MX2201 BLE discovery and connection
- command characteristic discovery
- notification enablement
- status pointer parsing
- dynamic 60-second logger interval reporting
- 64-byte memory reconstruction from four fragments
- packed 12-bit temperature decoding
- correct phase migration across moving memory windows
- rejection of false high-temperature phase candidates
- recency-first selection of newly logged data
- confirmed large cold-to-hot temperature transition
- confirmed large hot-to-cold temperature transition
- BLE connection remaining alive during slow logging
- standard Meshtastic environmental telemetry creation
- LoRa transmission
- reception by a second Meshtastic node
- Android displaying the received temperature
- removal of the duplicate initial telemetry send

## What is not implied by this document

This integration is specific to the tested MX2201 and the tested firmware/hardware configuration.

It does not prove that:

- every MX2201 has the same BLE MAC
- an MX2001 uses the same protocol or memory format
- future Meshtastic versions can accept these changes without adaptation
- another nRF52 target will behave identically

For another logger model such as the MX2001, create a separate development branch and preserve the stable MX2201 implementation unchanged.

## Recovery instructions

To return to the exact bench-proven firmware code:

```powershell
git fetch origin --tags
git checkout mx2201-stable-2026-08-13
```

That checkout is intentionally detached because the stable tag is immutable.

To resume normal MX2201 development from the current integration branch:

```powershell
git checkout mx2201-integration
git pull
```

Before making risky changes for a different logger model, create a new branch rather than modifying the stable tag or rewriting the proven history.
