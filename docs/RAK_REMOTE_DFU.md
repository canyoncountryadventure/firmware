# RAK4631 Remote Drone DFU — Architecture and Validation Notes

This document is the technical reference for the `Remote-Drone-Flashing` branch.

## Purpose

Update a remote RAK4631 Meshtastic + HOBO field node without opening its enclosure or attaching USB.

The field architecture uses three radios:

```text
Controller RAK
      |
      | LoRa direct message: DFU
      v
Target RAK4631
      |
      | reboot to AdaDFU
      v
Drone Scout RAK4631
      |
      | BLE Nordic Legacy DFU
      v
Target RAK4631
      |
      | LoRa result callback after reboot
      v
Controller RAK
```

The Scout is a dedicated BLE flasher. It does not run Meshtastic and does not need a LoRa antenna during the mission.

## Target firmware

The target is the normal field application. It includes:

- Meshtastic LoRa
- HOBO MX2001 / MX2201 / MX2203 support
- `NEWREAD64` next-record tracking
- direct-message control commands
- BLE DFU trigger
- persistent post-DFU callback marker
- nRF52840 internal watchdog through `HOBOSelfRecoveryModule`
- self-recovery / BLE scanner recovery

### VERSION

Direct-message:

```text
VERSION
```

The response intentionally stays well below Meshtastic's 233-byte Data payload ceiling and reports:

- radio/platform
- supported sensors
- NEXTREAD / NEWREAD64 state
- DM command support
- semantic version + Git SHA
- DFU state
- watchdog state and timeout
- compile date

### WATCHDOG

Direct-message:

```text
WATCHDOG
```

The self-recovery supervisor arms the nRF52840 on-chip watchdog after the 30-second boot-settle period.

Current configuration:

```text
timeout: 900 seconds / 15 minutes
run while CPU sleeps: yes
feed interval: approximately 30 seconds
```

The `WATCHDOG` reply reads the live WDT state.

## DFU trigger

The target accepts a direct Meshtastic text command:

```text
DFU
```

When received, it:

1. verifies the message is addressed directly to this node;
2. stores the requester node ID, channel, and current `APP_VERSION`;
3. replies that BLE OTA DFU is armed;
4. waits about three seconds;
5. writes `0xA8` to `NRF_POWER->GPREGRET`;
6. resets into the Adafruit/Nordic BLE OTA bootloader.

The target refuses to enter DFU if the callback marker cannot be written.

## Persistent callback marker

File:

```text
/prefs/dfu_pending.bin
```

Record:

| Offset | Length | Meaning |
|---|---:|---|
| 0–3 | 4 | Magic `DFU2` |
| 4 | 1 | Record format version 2 |
| 5–8 | 4 | requester node ID, little-endian |
| 9 | 1 | Meshtastic channel |
| 10 | 1 | saved build-ID length |
| 11–30 | 20 | saved pre-DFU `APP_VERSION` |
| 31 | 1 | XOR checksum over bytes 0–30 |

The marker survives an application-only DFU.

After the application boots:

### Different build

```text
UPDATE SUCCESS
RAK4631 booted new firmware after BLE DFU
Old: 2.7.26.<oldsha>
New: 2.7.26.<newsha>
```

### Same build

```text
DFU RESULT: BUILD UNCHANGED
Same build after reboot
Build: 2.7.26.<sha>
```

A same-build result is intentionally neutral. It cannot distinguish a successful reflash of identical firmware from returning to an identical build. DFU-side validation must be used for that case.

The marker is cleared only after the result packet is queued. If allocation fails, the target retries later.

## Tested bootloader

The tested RAK4631 bootloader advertises:

```text
Name: AdaDFU

Service:
00001530-1212-EFDE-1523-785FEABCD123

Control Point:
00001531-1212-EFDE-1523-785FEABCD123

Packet:
00001532-1212-EFDE-1523-785FEABCD123

Version:
00001534-1212-EFDE-1523-785FEABCD123
```

Observed behavior: the BLE address changes by +1 when the target enters bootloader mode.

## Autonomous Scout

Source:

```text
src/experimental/rak4631_drone_flasher.cpp
```

The Scout:

1. boots from battery power;
2. scans indefinitely for Legacy AdaDFU service `0x1530`;
3. requires RSSI of at least -90 dBm;
4. connects and discovers the DFU characteristics;
5. enables Control Point notifications;
6. performs application-only Nordic Legacy DFU;
7. streams a target image embedded in its own internal flash;
8. stops after a validated activation sequence.

No PC, SD card, ESP32, USB cable, or LoRa stack is required in flight.

### BLE transfer parameters

```text
application type: 0x04
BLE payload: 20 bytes
packet receipt notification interval: 8 packets
SoftDevice size: 0
bootloader size: 0
application size: embedded target BIN size
```

Sequence:

1. `START_DFU`
2. image sizes
3. `INIT_DFU_PARAMS` start
4. application `.dat`
5. `INIT_DFU_PARAMS` complete
6. PRN interval = 8
7. `RECEIVE_FW`
8. application BIN
9. `VALIDATE`
10. `ACTIVATE_AND_RESET`

## Embedded image

The target build produces an application-only OTA ZIP containing:

- `manifest.json`
- application `.dat`
- application `.bin`

`tools/build_drone_embedded_firmware.py`:

1. extracts the application files;
2. compresses the BIN with raw LZ4 block compression;
3. verifies decompression with the Python LZ4 library;
4. verifies decompression again with an implementation matching the Scout's exact 64 KiB circular-history decoder;
5. generates `src/experimental/embedded_dfu_image.h`.

The build fails if either verification does not reproduce the original target BIN byte-for-byte.

## Proven autonomous bench test

The autonomous Scout has completed the full embedded-image update path. The successful retained reference pair is:

```text
Target: RAK4631-HOBO-DFU-Target-2.7.26.b812974.uf2
Scout:  RAK4631-Remote-Drone-Flasher-embeds-2.7.26.b812974.uf2
```

The target and Scout came from the same `remote-drone-flashing-12` workflow artifact. The Scout embeds target build `2.7.26.b812974`.

Observed Scout output:

```text
STREAMING EMBEDDED FIRMWARE...
PROGRESS 0%
PROGRESS 5%
...
PROGRESS 95%
PROGRESS 100%
VALIDATED; ACTIVATE SENT
BLE DISCONNECTED reason=0x13
AUTONOMOUS DFU SUCCESS
```

The target then rebooted into Meshtastic and sent the stored callback.

This proves the autonomous path through:

```text
mesh DFU trigger
→ persistent callback marker
→ AdaDFU
→ Scout discovery
→ full embedded image transfer
→ RECEIVE_FW
→ VALIDATE
→ ACTIVATE
→ disconnect/reset
→ Meshtastic application reboot
→ LoRa callback
```

The initial autonomous test sequence included a same-build reflash, which correctly exercised the neutral `DFU RESULT: BUILD UNCHANGED` path. The retained `b812974` pair is the successful reference firmware. After autonomous flashing was working, the remaining change was compacting the `VERSION` DM so the radio, sensors, NEXTREAD mode, DM commands, build, DFU state, watchdog and compile date fit in one reliable Meshtastic response.

## LED behavior

### Scout

- brief blink about every two seconds: scanning/waiting;
- BLE/DFU activity: transfer in progress;
- solid LED: Scout completed the DFU sequence successfully.

### Target

A brief blue LED flash can occur when a direct Meshtastic message is received or processed. That alone does **not** mean the target entered DFU. DFU mode is identified by the explicit `DFU` command/reply followed by loss of normal Meshtastic operation while `AdaDFU` is active.

## Recovery

The updater is application-only. It does not intentionally replace the SoftDevice or bootloader.

An interrupted transfer can leave the application invalid. USB UF2 recovery remains the development fallback.

Use the matching target UF2 generated by the same workflow.

## Build system

Workflow:

```text
.github/workflows/build_remote_drone_flasher.yml
```

The workflow builds both sides from the same commit:

1. target `rak4631`;
2. target application-only OTA ZIP;
3. generated embedded LZ4 header;
4. autonomous `rak4631_drone_flasher`;
5. matched artifacts.

The workflow is triggered by source and RAK configuration changes that can affect either the target or the Scout. This is intentional: a Scout artifact must never silently contain a stale target image after target source changes.

## Output naming

Versioned files:

```text
RAK4631-HOBO-DFU-Target-<version>.uf2
RAK4631-HOBO-DFU-Target-<version>-OTA.zip
RAK4631-Remote-Drone-Flasher-embeds-<version>.uf2
```

Stable aliases are also packaged:

```text
RAK4631-HOBO-DFU-Target.uf2
RAK4631-HOBO-DFU-Target-OTA.zip
RAK4631-Remote-Drone-Flasher.uf2
```

`BUILD.txt` records the commit, target version, file sizes, and explicitly states which target version is embedded in the Scout.

## Source map

- [Autonomous Scout](../src/experimental/rak4631_drone_flasher.cpp)
- [Embedded-image generator](../tools/build_drone_embedded_firmware.py)
- [Target HOBO + DFU](../src/modules/Telemetry/HOBOMX2001MX2201MX2203/HOBOMX2001MX2201MX2203Telemetry.cpp)
- [RAK wrapper](../src/modules/Telemetry/HOBOMX2001MX2201MX2203/HOBOMX2001MX2201MX2203TelemetryRAK.cpp)
- [Self-recovery watchdog](../src/modules/Telemetry/HOBOSelfRecovery/HOBOSelfRecovery.cpp)
- [RAK module attachment](../src/modules/Telemetry/MX2001Diagnostic.h)
- [RAK PlatformIO configuration](../variants/nrf52840/rak4631/platformio.ini)
- [Integrated workflow](../.github/workflows/build_remote_drone_flasher.yml)
