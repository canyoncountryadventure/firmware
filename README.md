# Remote Drone Flashing

Autonomous **RAK4631-to-RAK4631 firmware updating over BLE** for remote Meshtastic field stations.

The field design uses three radios:

```text
Controller RAK                  Target RAK4631
(with operator)                    field node
      |                               |
      | LoRa DM: DFU                  | reboots into AdaDFU
      +------------------------------>|
                                      |
                                      | BLE Legacy DFU
                                      v
                               Drone Scout RAK4631
                               DFU-only firmware
                               target image embedded
```

The **drone Scout does not need a laptop, phone, SD card, ESP32, LoRa antenna, or USB connection during flight**.

## Hardware

### Drone Scout

- RAK4631
- RAK19007
- BLE antenna
- small 3.7 V LiPo

Measured RAK19007 + RAK4631 + BLE antenna: **10.4 g**.

With a ~3 g 100 mAh LiPo and lightweight mounting, the complete Scout should be roughly **14–15 g**.

### Controller

Any normal Meshtastic radio that can directly message the target.

### Target

RAK4631 running the confirmation-capable Meshtastic + HOBO target firmware from this branch.

## How it works

1. Flash **RAK4631-Remote-Drone-Flasher.uf2** onto the drone Scout.
2. Power the Scout from its battery before takeoff.
3. Fly the Scout near the target.
4. From the separate controller radio, send the target:
   ```text
   DFU
   ```
5. The target stores the requesting controller node and current build ID, replies that DFU is armed, and reboots into **AdaDFU**.
6. The drone Scout automatically detects the AdaDFU BLE service.
7. The Scout decompresses the target firmware stored inside its own internal flash and streams the application to the target over BLE.
8. The target validates the image, activates it, and reboots into Meshtastic.
9. The target sends the original controller a result message.

Successful new build:

```text
UPDATE SUCCESS
RAK4631 booted new firmware after BLE DFU
Old: 2.7.26.<oldsha>
New: 2.7.26.<newsha>
```

If the old firmware simply resumes without a different build being installed:

```text
DFU NOT CONFIRMED
Previous firmware resumed
Build: 2.7.26.<sha>
```

## Why the firmware fits inside the Scout

The RAK4631 drone firmware is intentionally small and does **not** run Meshtastic.

The target application is compressed with **LZ4** during the build and embedded directly into the Scout firmware. During DFU, the Scout keeps only a 64 KiB LZ4 history window in RAM and streams decompressed bytes directly to BLE.

There is no second uncompressed 780 KB image stored in RAM or external storage.

## Drone Scout behavior

Power-on behavior is automatic:

```text
Power on
   ↓
scan for AdaDFU
   ↓
connect when target appears
   ↓
Legacy DFU START
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

LED behavior:

- Brief blink every ~2 seconds: scanning/waiting.
- Active BLE/DFU: normal runtime activity.
- Solid LED: DFU completed successfully.

USB serial output is retained for bench diagnostics but is **not required in flight**.

## Build outputs

The integrated GitHub Actions workflow builds both sides from the same commit:

- **RAK4631-Remote-Drone-Flasher.uf2** — goes on the drone Scout.
- **RAK4631-Target-Confirmation-Firmware.uf2** — USB/recovery target firmware.
- **RAK4631-Target-Confirmation-Firmware-OTA.zip** — target application OTA package.

[Build Remote Drone Flasher workflow](.github/workflows/build_remote_drone_flasher.yml)

[Remote Drone Flashing Actions](https://github.com/canyoncountryadventure/firmware/actions/workflows/build_remote_drone_flasher.yml?query=branch%3ARemote-Drone-Flashing)

## Source

- [Autonomous drone flasher](src/experimental/rak4631_drone_flasher.cpp)
- [Embedded-image generator](tools/build_drone_embedded_firmware.py)
- [Target DFU + HOBO implementation](src/modules/Telemetry/HOBOMX2001MX2201MX2203/HOBOMX2001MX2201MX2203Telemetry.cpp)
- [RAK4631 PlatformIO environments](variants/nrf52840/rak4631/platformio.ini)
- [Architecture / development notes](docs/RAK_REMOTE_DFU.md)

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
```

The Scout performs an **application-only Nordic Legacy DFU**. It does not intentionally replace the target bootloader or SoftDevice.

## Recovery

An interrupted update can leave the target application invalid, because Legacy DFU erases/replaces the application region.

The target bootloader should remain intact. During development, keep the target USB UF2 available so the node can be restored manually if needed.

## Development history

The earlier `esp-32-testing` branch proved the complete PC-assisted transfer:

- Mesh `DFU` command entered AdaDFU.
- Scout discovered Legacy DFU.
- Full ~780 KB application transferred over BLE.
- `RECEIVE_FW` returned success.
- `VALIDATE` returned success.
- `ACTIVATE` rebooted the target successfully.

This branch replaces the PC serial bridge with **firmware stored inside the drone Scout itself**.
