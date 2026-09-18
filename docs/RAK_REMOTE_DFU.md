# RAK4631 Remote DFU — Architecture and Bench Notes

This document describes the experimental remote application-update path on the `esp-32-testing` branch.

## Goal

Update a deployed RAK4631 without opening the enclosure or attaching USB, using a nearby RAK4631 as the BLE DFU client.

The target remains a normal Meshtastic + HOBO field node during normal operation.

## Target behavior

The target listens for a direct Meshtastic text command:

```text
DFU
```

When received, the target:

1. Verifies the command is addressed directly to this node.
2. Persists the requester node ID and channel to `/prefs/dfu_pending.bin`.
3. Replies that DFU is armed.
4. Waits three seconds.
5. Writes `0xA8` to `NRF_POWER->GPREGRET`.
6. Resets.
7. Boots the Adafruit/Nordic BLE OTA bootloader.

The tested bootloader advertises as `AdaDFU`.

## Persistent callback marker

The callback record intentionally lives outside the application image so it survives an application-only OTA update.

Record layout:

| Offset | Length | Meaning |
|---|---:|---|
| 0–3 | 4 | Magic `DFU1` |
| 4 | 1 | Record version |
| 5–8 | 4 | Requester node ID, little-endian |
| 9 | 1 | Meshtastic channel |
| 10 | 1 | Reserved |
| 11 | 1 | XOR checksum of bytes 0–10 |

The target refuses to enter DFU if this record cannot be written.

After the updated application boots, the HOBO/DFU module loads the marker, waits for normal Meshtastic startup, and queues:

```text
UPDATE SUCCESS
RAK4631 application booted after BLE DFU
Meshtastic 2.7.26 + HOBO
```

Only after that text packet is successfully allocated and queued is the marker deleted.

If packet allocation fails, the marker remains and another send is scheduled 30 seconds later.

## Observed bootloader

The bootloader used by the tested RAK4631 exposes Nordic Legacy DFU:

```text
Device name:
  AdaDFU

Service:
  00001530-1212-EFDE-1523-785FEABCD123

Control Point:
  00001531-1212-EFDE-1523-785FEABCD123

Packet:
  00001532-1212-EFDE-1523-785FEABCD123

Version:
  00001534-1212-EFDE-1523-785FEABCD123
```

The target BLE address was observed to change by +1 when entering DFU mode.

## Scout behavior

The standalone Scout firmware is a BLE-central-only RAK4631 application.

It:

1. Scans for nearby BLE advertisements.
2. Detects Legacy DFU `0x1530` and keeps Secure DFU `0xFE59` recognition as a compatibility fallback.
3. Connects to `AdaDFU`.
4. Discovers the DFU service and required characteristics.
5. Enables Control Point notifications.
6. Exposes a simple USB serial bridge protocol to the PC helper.
7. Performs Nordic Legacy application DFU.

### Legacy DFU sequence

The Scout sends:

1. `START_DFU` (`0x01`) with application type `0x04`.
2. Three little-endian image sizes: SoftDevice = 0, bootloader = 0, application = BIN size.
3. `INIT_DFU_PARAMS` start.
4. The `.dat` init packet.
5. `INIT_DFU_PARAMS` complete.
6. Packet Receipt Notification interval = 8 packets.
7. `RECEIVE_FW`.
8. The complete application `.bin`.
9. `VALIDATE`.
10. `ACTIVATE_AND_RESET`.

The current compatibility-first transfer uses 20-byte BLE writes.

## PC serial bridge

`tools/rak_dfu_serial_upload.py` opens a standard Nordic OTA ZIP and reads:

- `manifest.json`
- the application `.dat`
- the application `.bin`

The Scout asks the PC for blocks using lines such as:

```text
REQ DAT 0 14
REQ BIN 0 1024
REQ BIN 1024 1024
```

The Python helper immediately returns exactly that many raw bytes over USB CDC.

The Scout then packetizes those bytes into 20-byte BLE writes.

## Verified Phase 2 run

The tested OTA bundle contained:

```text
DAT: 14 bytes
BIN: 778,048 bytes
```

The complete transfer reached:

```text
PROGRESS 100% 778048/778048
DFU STREAM COMPLETE
RECEIVE_FW status=0x1
VALIDATE status=0x1
ACTIVATE sent
DISCONNECTED, reason 0x13
DFU SUCCESS: target accepted image and rebooted
```

The target then booted the flashed Meshtastic + HOBO application.

### Fixed transfer bug

An early Phase 2 build truncated the remaining firmware byte count to `uint16_t` before capping the PC request to 1,024 bytes.

That caused this exact failure:

```text
REQ BIN 56320 832
REQ BIN 57152 0
```

The corrected code keeps the subtraction in 32 bits and converts to 16 bits only after applying the 1,024-byte cap.

A zero-length block guard was also added.

## Recovery model

The current updater is application-only.

It intentionally sends:

```text
SoftDevice size: 0
Bootloader size: 0
Application size: <BIN size>
```

Therefore an interrupted transfer can invalidate the Meshtastic application, but it should not intentionally overwrite the bootloader or SoftDevice.

During development, keep a known-good UF2 available for USB recovery.

## Current bench topology

```text
Normal Meshtastic sender
        |
        | LoRa "DFU"
        v
Target RAK4631
        |
        | AdaDFU BLE
        v
Scout RAK4631
        |
        | USB serial
        v
PC + OTA ZIP
```

The third Meshtastic sender is still required in this **bench implementation** because the standalone Scout firmware does not currently run the Meshtastic stack.

## Final two-RAK topology

The intended field implementation merges the Scout DFU client into normal Meshtastic firmware:

```text
Phone / controller
        |
        | BLE / serial
        v
Scout RAK4631 running Meshtastic
        |
        | LoRa "DFU"
        v
Target RAK4631
        |
        | AdaDFU
        v
Scout RAK4631
        |
        | BLE firmware update
        v
Target RAK4631
        |
        | LoRa UPDATE SUCCESS
        v
Scout / controller
```

No ESP32 is required for this RAK-to-RAK firmware-update path.

## Next integration work

1. Merge the Legacy DFU client into a normal RAK4631 Meshtastic build.
2. Give the Scout local firmware storage or another non-PC firmware source.
3. Add explicit target selection so a Scout never flashes the wrong nearby `AdaDFU` device.
4. Add update identity/version metadata to the command and success response.
5. Add timeout/failure messages over Meshtastic from the integrated Scout.
6. Repeat the full test without a PC participating in the actual transfer.

## Source map

- [Scout implementation](../src/experimental/rak4631_dfu_scout.cpp)
- [Target HOBO + DFU command implementation](../src/modules/Telemetry/HOBOMX2001MX2201MX2203/HOBOMX2001MX2201MX2203Telemetry.cpp)
- [RAK wrapper](../src/modules/Telemetry/HOBOMX2001MX2201MX2203/HOBOMX2001MX2201MX2203TelemetryRAK.cpp)
- [PC uploader](../tools/rak_dfu_serial_upload.py)
- [Scout workflow](../.github/workflows/build_rak_dfu_scout.yml)
- [Target workflow](../.github/workflows/build_rak_ble_dfu_target.yml)
- [RAK4631 PlatformIO configuration](../variants/nrf52840/rak4631/platformio.ini)
