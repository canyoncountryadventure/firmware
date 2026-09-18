> **Remote Drone Flashing v2:** the proven embedded-flasher workflow is retained, but its embedded RAK target now carries the Field-Recovery v2 radio/watchdog core and HOBO recovery stack.

# Remote Drone Flashing v2

Autonomous **RAK4631-to-RAK4631 firmware updating over BLE** for remote Meshtastic field stations.

**Validation status:** the original matched `2.7.26.b812974` target + Scout pair completed the full physical DFU transfer, validation, activation, reboot and return to Meshtastic. The current hardened v2 pair preserves that protocol but has changed target recovery/persistence code, so it must receive one repeat end-to-end bench DFU before replacing the proven pair in remote field use.

This branch builds a matched pair:

- **Target firmware** — normal Meshtastic + HOBO field firmware with remote DFU, self-recovery, watchdog, version reporting, and post-update callback.
- **Drone Scout firmware** — BLE-only RAK4631 firmware carrying a compressed copy of that exact target application in its own internal flash.

The Scout needs no laptop, phone, SD card, ESP32, LoRa antenna, or USB connection during flight.

## Project and download links

| Item | Link |
|---|---|
| Source branch | [Remote-Drone-Flashing-v2](https://github.com/canyoncountryadventure/firmware/tree/Remote-Drone-Flashing-v2) |
| **Verified successful `b812974` build** | **[Workflow run and `remote-drone-flashing-12` artifact](https://github.com/canyoncountryadventure/firmware/actions/runs/35312994634)** |
| **Current v2 target UF2** | **[RAK4631-HOBO-DFU-Target-v2.uf2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK4631-HOBO-DFU-Target-v2.uf2)** |
| **Current v2 target BLE DFU ZIP** | **[RAK4631-HOBO-DFU-Target-v2-OTA.zip](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK4631-HOBO-DFU-Target-v2-OTA.zip)** |
| **Current v2 Scout UF2** | **[RAK4631-Remote-Drone-Flasher-v2.uf2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK4631-Remote-Drone-Flasher-v2.uf2)** |
| **Matched build workflow** | **[Latest Remote Drone Flasher workflow runs](https://github.com/canyoncountryadventure/firmware/actions/workflows/build_remote_drone_flasher.yml?query=branch%3ARemote-Drone-Flashing-v2)** |
| Workflow source | [build_remote_drone_flasher.yml](.github/workflows/build_remote_drone_flasher.yml) |
| Technical design | [RAK_REMOTE_DFU.md](docs/RAK_REMOTE_DFU.md) |
| Scout source | [rak4631_drone_flasher.cpp](src/experimental/rak4631_drone_flasher.cpp) |
| Target HOBO + DFU source | [HOBOMX2001MX2201MX2203Telemetry.cpp](src/modules/Telemetry/HOBOMX2001MX2201MX2203/HOBOMX2001MX2201MX2203Telemetry.cpp) |

For the **verified successful pair**, open the `b812974` workflow-run link and download its `remote-drone-flashing-12` artifact. Use the general workflow-runs link only for newer development builds; a green CI result proves that the pair built and packaged correctly, not that the new commit has repeated the physical end-to-end test. GitHub may require sign-in to download Actions artifacts. Each artifact contains the target UF2, target OTA ZIP, Scout UF2 and `BUILD.txt`.

**Keep the target and Scout files from the same workflow artifact together.** The Scout contains a compressed copy of that exact target application. Mixing files from different runs defeats the matched-pair design.

### Verified successful pair

| File | SHA-256 |
|---|---|
| `RAK4631-HOBO-DFU-Target-2.7.26.b812974.uf2` | `ccaf267071f7c05ca354c98bb3fea39f0be7471673ab345b79342352db7feb99` |
| `RAK4631-Remote-Drone-Flasher-embeds-2.7.26.b812974.uf2` | `ead3da235a1e96d14ecfc4afd9a120721cd71a0bdf7e488588a2fa95fd37fd27` |

The target UF2 contains build `2.7.26.b812974` and the compact `RADIO / SENSORS / NEXTREAD / DM / BUILD / DFU / WDT / DATE` response. The Scout UF2 identifies the same embedded target build and contains the autonomous `AdaDFU` transfer path.

## Current validation status

| Capability | Status |
|---|---|
| LoRa `DFU` command stores the requester/build marker and reboots target into `AdaDFU` | **Bench proven** |
| Scout finds `AdaDFU`, transfers the full embedded image, validates, activates and triggers reboot | **Bench proven** |
| Target returns to Meshtastic and sends the stored LoRa callback | **Bench proven** |
| Original matched `2.7.26.b812974` target + Scout firmware pair | **Successfully tested** |
| Current hardened v2 target + Scout pair | **Source-integrated and CI-rebuilt; physical end-to-end DFU should be revalidated before remote deployment** |
| Compact `VERSION` response with every required field under the payload limit | **Bench proven in `b812974`; restored in current hardened v2 source** |
| Physical drone flight/hover | Separate operational test; the autonomous firmware-update chain itself is proven |
| Seeed XIAO targets | **Not supported by this branch** |

The original matched pair proves the target/Scout DFU protocol. The v2 branch preserves that protocol while changing the target recovery core, so the new matched v2 pair should receive one repeat bench DFU before it replaces the proven `b812974` pair in the field.

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

The original validation sequence included same-build testing, where the callback correctly reported that the build ID was unchanged. The later matched `b812974` pair is the successful reference build retained above. The callback wording remains deliberately precise so it never claims a version change when the old and new build IDs are identical.

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
- dual-channel nRF52840 watchdog protection
- automatic HOBO BLE recovery with 30-second connect cancellation and bounded STATUS/NEWREAD retries

### Controller

Any normal Meshtastic radio that can directly message the target.

## Field procedure

1. Download one successful workflow artifact and read its `BUILD.txt`.
2. Flash its **target UF2** onto the field target.
3. Flash the **Drone Flasher UF2 from that same artifact** onto the Scout.
4. Boot the target and direct-message `VERSION`; record the current `BUILD:` line.
5. Confirm the target answers `WATCHDOG`, `LOGGER` and `PING` normally.
6. Bench-power the Scout and confirm its waiting blink before attaching it to the drone.
7. Confirm the matching target UF2 is available for USB recovery before leaving the bench.
8. Power the Scout before takeoff.
9. Fly the Scout close to the target.
10. Hover near the target.
11. Direct-message the target:

   ```text
   DFU
   ```

12. The target stores the requesting controller node, channel, and current build ID.
13. The target replies that DFU is armed.
14. About three seconds later the target flushes pending flash writes, disables the active SoftDevice, sets the bootloader flag, and resets into `AdaDFU`.
15. The Scout detects `AdaDFU`, connects, and flashes its embedded target image.
16. The target validates and activates the application.
17. The target reboots into Meshtastic.
18. If the build changed, the controller receives `UPDATE SUCCESS` with old and new build IDs.

For field use, keep the Scout near the target until the Scout shows solid-success and the controller receives the expected post-update callback. Do not treat a brief target LED flash as proof that DFU started.

## Flashing the matched pair at the bench

### Target RAK4631

Use `RAK4631-HOBO-DFU-Target-v2.uf2` for USB installation. Double-press reset, copy the UF2 to the RAK4631 bootloader drive, and let it reboot.

`RAK4631-HOBO-DFU-Target-v2-OTA.zip` is the phone/nRF Connect alternative. Select the unopened ZIP as a **Distribution packet (ZIP)**; do not extract it and do not select the UF2 in nRF Connect.

### Scout RAK4631

Use `RAK4631-Remote-Drone-Flasher-v2.uf2`. Double-press reset and copy it to the Scout's RAK4631 bootloader drive. Once flashed, the Scout is a dedicated BLE flasher—not a Meshtastic node.

## Target commands

### VERSION

Direct-message:

```text
VERSION
```

The response is intentionally kept comfortably below Meshtastic's **233-byte Data payload limit** so it remains reliable over a weak link.

For the RAK target it reports this compact format:

```text
RADIO:RAK4631/19007
SENSORS:HOBO MX2001/2201/2203
NEXTREAD:ON NEWREAD64
DM:ON VERSION DFU LOGGER LOCK UNLOCK READ
BUILD:2.7.26.<gitsha>
DFU:ON
WDT:90s+field
DATE:<compile date>
```

The RAK response is about **176 bytes**, leaving substantial margin below the 233-byte Meshtastic Data payload ceiling and avoiding the longer response that proved unreliable in the phone client.

A brief target LED flash when a direct message is received/processed is normal radio/status activity; it does not by itself mean DFU mode was entered.

### WATCHDOG

Direct-message:

```text
WATCHDOG
```

This reads the live self-recovery watchdog state.

The hardened v2 RAK target uses the **Field-Recovery v2 nRF52840 watchdog model**:

- main-loop hardware watchdog: **90 seconds**
- independent field-health watchdog channel: **armed**
- both channels continue during CPU sleep/halt
- repeated unrecoverable SX1262 or HOBO BLE failures can deliberately starve the field channel and force a hardware reset
- a **12-hour preventive reboot** remains enabled during the v2 burn-in period

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

`SCAN` and `RECONNECT` are retained as legacy diagnostic commands in v2; BLE scanner/link recovery is automatic and remains owned by the HOBO state machine.

## Command authorization and security

The target currently checks that `DFU` is a successfully decoded direct text message addressed to that node. It does **not** maintain a separate DFU sender allowlist or require a Meshtastic remote-admin key inside this command handler.

Therefore, any mesh node whose direct message the target can successfully decrypt may be able to arm DFU. Deploy the target only on the intended private channel/key arrangement, protect node private keys, and do not advertise this command on a public channel. A sender allowlist is future hardening, not a current feature.

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

If the target returns to `AdaDFU` but no application boots, land/recover the Scout and install the matching target UF2 over USB. Do not factory-erase unless the intent is to remove the node's identity, channel configuration, keys and application state.

If the target gives three flashes followed by a pause repeatedly, it is in the nRF52840 low-VDD safe-boot loop, not waiting for the Scout. Test it from a known-good 5-V USB-C source with battery, solar and accessories disconnected, then reseat the RAK4631 if the pattern remains.

## Source

- [Autonomous drone flasher](src/experimental/rak4631_drone_flasher.cpp)
- [Embedded-image generator](tools/build_drone_embedded_firmware.py)
- [Target HOBO + DFU implementation](src/modules/Telemetry/HOBOMX2001MX2201MX2203/HOBOMX2001MX2201MX2203Telemetry.cpp)
- [RAK target wrapper](src/modules/Telemetry/HOBOMX2001MX2201MX2203/HOBOMX2001MX2201MX2203TelemetryRAK.cpp)
- [Self-recovery watchdog](src/modules/Telemetry/HOBOSelfRecovery/HOBOSelfRecovery.cpp)
- [RAK module attachment](src/modules/Telemetry/MX2001Diagnostic.h)
- [RAK4631 PlatformIO environment](variants/nrf52840/rak4631/platformio.ini)
- [Technical architecture and validation notes](docs/RAK_REMOTE_DFU.md)
