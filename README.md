# Remote Drone Flashing

Autonomous **RAK4631-to-RAK4631 firmware updating over BLE** for remote Meshtastic field stations.

This branch builds a matched pair:

- **Target firmware** — normal Meshtastic + HOBO field firmware with remote DFU, self-recovery, watchdog, version reporting, and post-update callback.
- **Drone Scout firmware** — BLE-only RAK4631 firmware carrying a compressed copy of that exact target application in its own internal flash.

The Scout needs no laptop, phone, SD card, ESP32, LoRa antenna, or USB connection during flight.

## Architecture

```text
Controller RAK
(operator / phone)
      |
      | LoRa direct message: DFU
      v
Target RAK4631
Meshtastic + HOBO + DFU
      |
      | reboot into AdaDFU
      v
Drone Scout RAK4631
BLE-only autonomous flasher
      |
      | Nordic Legacy DFU over BLE
      v
Target RAK4631
new application boots
      |
      | LoRa callback
      v
Controller RAK
```

The controller and Scout are separate radios. The Scout does not run Meshtastic; it only performs the nearby BLE firmware transfer.

## Proven autonomous bench test

The autonomous Scout has completed a full embedded-image DFU transfer through:

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

The target then rebooted into Meshtastic and sent its stored post-DFU callback.

The first fully autonomous validation used the same build before and after the transfer. That proves the transfer/validate/activate/reboot path, but a same-build callback cannot prove a version change.

Current callback wording is therefore deliberately precise.

### Different build installed

```text
UPDATE SUCCESS
RAK4631 booted new firmware after BLE DFU
Old: 2.7.26.<oldsha>
New: 2.7.26.<newsha>
```

### Same build after reboot

```text
DFU RESULT: BUILD UNCHANGED
Same build after reboot
Build: 2.7.26.<sha>
```

A same-build result is **not automatically a DFU failure**. The Scout's `VALIDATED; ACTIVATE SENT`, expected BLE disconnect, and autonomous success message are the DFU-side evidence.

## Hardware

### Drone Scout

- RAK4631
- RAK19007
- BLE antenna
- small 3.7 V LiPo

Measured RAK19007 + RAK4631 + BLE antenna: **10.4 g**.

With a ~3 g 100 mAh LiPo and lightweight mounting, the complete Scout should be roughly **14–15 g**.

### Target

RAK4631 / RAK19007 running the target firmware from this branch.

Target capabilities:

- Meshtastic LoRa
- HOBO MX2001 / MX2201 / MX2203
- `NEWREAD64` next-record tracking
- direct-message commands
- BLE DFU trigger
- persistent post-update callback
- nRF52840 internal watchdog
- BLE/self-recovery supervisor

### Controller

Any normal Meshtastic radio that can directly message the target.

## Field procedure

1. Flash the latest matched **target UF2** onto the field target.
2. Flash the matching **Drone Flasher UF2** onto the Scout.
3. Power the Scout before takeoff.
4. Fly the Scout close to the target.
5. Hover near the target.
6. Direct-message the target:

   ```text
   DFU
   ```

7. The target stores the requesting controller node, channel, and current build ID.
8. The target replies that DFU is armed.
9. About three seconds later the target resets into `AdaDFU`.
10. The Scout detects `AdaDFU`, connects, and flashes its embedded target image.
11. The target validates and activates the application.
12. The target reboots into Meshtastic.
13. If the build changed, the controller receives `UPDATE SUCCESS`.

For field use, keep the Scout near the target until the expected post-update callback arrives.

## Target commands

### VERSION

Direct-message:

```text
VERSION
```

The response is intentionally kept comfortably below Meshtastic's **233-byte Data payload limit** so it remains reliable over a weak link.

It reports:

```text
RADIO: RAK4631/RAK19007
SENSORS: HOBO MX2001/MX2201/MX2203
NEXTREAD: ON (NEWREAD64)
DM: ON VERSION,DFU,LOGGER,LOCK,UNLOCK,READ
BUILD: 2.7.26.<gitsha>
DFU: ON
WDT: ON (nRF52840 internal, 900s)
DATE: <compile date>
```

A brief target LED flash when a direct message is received/processed is normal radio/status activity; it does not by itself mean DFU mode was entered.

### WATCHDOG

Direct-message:

```text
WATCHDOG
```

This reads the live self-recovery watchdog state.

The RAK target uses the **nRF52840 internal WDT**:

- boot settle before arming: **30 seconds**
- timeout: **900 seconds / 15 minutes**
- runs while CPU sleeps
- supervisor feeds it approximately every 30 seconds

### Other target commands

```text
LOGGER
LOCK
UNLOCK
READ
DFU
HELP
STATUS
HEALTH
POWER
BLE
AUTO
STATS
NODES
UPTIME
WATCHDOG
SCAN
RECONNECT
RECOVER
REBOOT
PING
```

## Drone Scout behavior

```text
Power on
   ↓
scan for AdaDFU
   ↓
connect
   ↓
START_DFU
   ↓
send init packet
   ↓
stream embedded firmware
   ↓
VALIDATE
   ↓
ACTIVATE + target reboot
   ↓
Scout stops after success
```

Scout LED:

- brief blink about every 2 seconds: scanning/waiting
- BLE/DFU activity: transfer in progress
- solid LED: Scout completed the DFU sequence successfully

USB serial output is retained for bench diagnostics but is **not required in flight**.

## Why the target firmware fits inside the Scout

The Scout does not carry a second uncompressed ~780 KB application.

During CI:

1. the target application-only OTA ZIP is built;
2. its `.bin` is compressed with raw LZ4;
3. reference decompression is verified byte-for-byte;
4. a second decoder matching the Scout's exact **64 KiB circular-history** implementation verifies it again;
5. the compressed target and `.dat` init packet are compiled directly into the Scout.

During flight, the Scout decompresses and streams bytes directly to BLE.

## DFU protocol

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

The Scout performs **application-only Nordic Legacy DFU**.

Current compatibility-first transfer settings:

```text
BLE payload: 20 bytes
PRN interval: 8 packets
SoftDevice image: none
Bootloader image: none
Application image: embedded target BIN
```

## Build system

The integrated workflow is:

[Build Remote Drone Flasher](.github/workflows/build_remote_drone_flasher.yml)

It builds the target first, embeds that exact target OTA image into the Scout, builds the Scout, validates minimum artifact sizes, and uploads the matched pair.

The workflow watches target, Scout, self-recovery, common source, and RAK configuration changes so a Scout cannot silently carry a stale target image after firmware changes.

### Versioned outputs

```text
RAK4631-HOBO-DFU-Target-<version>.uf2
RAK4631-HOBO-DFU-Target-<version>-OTA.zip
RAK4631-Remote-Drone-Flasher-embeds-<version>.uf2
```

### Stable aliases

```text
RAK4631-HOBO-DFU-Target.uf2
RAK4631-HOBO-DFU-Target-OTA.zip
RAK4631-Remote-Drone-Flasher.uf2
```

`BUILD.txt` records the commit, target version, target/Scout sizes, and which target version is embedded in the Scout.

## Recovery

The update is application-only. It does not intentionally replace the SoftDevice or bootloader.

An interrupted transfer can leave the application invalid. During development, keep the matching target UF2 available for USB recovery.

## Source

- [Autonomous drone flasher](src/experimental/rak4631_drone_flasher.cpp)
- [Embedded-image generator](tools/build_drone_embedded_firmware.py)
- [Target HOBO + DFU implementation](src/modules/Telemetry/HOBOMX2001MX2201MX2203/HOBOMX2001MX2201MX2203Telemetry.cpp)
- [RAK target wrapper](src/modules/Telemetry/HOBOMX2001MX2201MX2203/HOBOMX2001MX2201MX2203TelemetryRAK.cpp)
- [Self-recovery watchdog](src/modules/Telemetry/HOBOSelfRecovery/HOBOSelfRecovery.cpp)
- [RAK module attachment](src/modules/Telemetry/MX2001Diagnostic.h)
- [RAK4631 PlatformIO environment](variants/nrf52840/rak4631/platformio.ini)
- [Technical architecture and validation notes](docs/RAK_REMOTE_DFU.md)
