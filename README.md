# RAK4631 Remote DFU / Drone Flasher

Experimental remote-firmware-update work for **RAK4631 / nRF52840 Meshtastic field nodes**.

This branch proves that a field RAK4631 can be commanded over Meshtastic to enter its Adafruit/Nordic BLE DFU bootloader, then receive a complete application update from a nearby RAK4631 over Bluetooth.

> **Branch:** `esp-32-testing`  
> **Meshtastic base:** 2.7.26  
> **Target hardware:** RAK4631 on RAK19007 / compatible WisBlock base  
> **DFU protocol actually used by the target:** Nordic **Legacy DFU** / `AdaDFU` (`0x1530` service)

## Current status

| Capability | Status |
|---|---|
| Direct Meshtastic `DFU` command to target | ✅ Bench verified |
| Target reboot into `AdaDFU` from software | ✅ Bench verified |
| Scout detects `AdaDFU` | ✅ Bench verified |
| Scout connects to Legacy DFU service `0x1530` | ✅ Bench verified |
| Transfer init packet (`.dat`) | ✅ Bench verified |
| Transfer full application image (`.bin`) | ✅ **778,048-byte image verified** |
| Nordic RECEIVE / VALIDATE / ACTIVATE / RESET | ✅ Bench verified |
| Target boots flashed Meshtastic + HOBO application | ✅ Bench verified |
| Persist requester/build before DFU and report verified new-build boot over mesh | ✅ Compiled + artifact verified; bench callback test is next |
| Scout carries firmware without a PC | ⏳ Next step |
| Scout runs normal Meshtastic and DFU client in one image | ⏳ Next step |
| Final two-RAK field architecture | ⏳ Integration step |

The successful Phase 2 bench run reached `100%`, returned `RECEIVE_FW status=0x1`, `VALIDATE status=0x1`, sent `ACTIVATE`, disconnected during reboot, and reported `DFU SUCCESS`.

### Latest validated CI artifacts

- **Target build:** `2.7.26.ba278ae`
  - Full UF2: **1,560,576 bytes**
  - Application OTA ZIP: **781,005 bytes**
  - Compiled image contains the build-aware `UPDATE SUCCESS` / `DFU NOT CONFIRMED` callback logic.
- **Scout build:** triggering commit `5d336b7`
  - Phase 2 serial-bridge UF2: **249,344 bytes**
  - Packaging is pinned to the triggering SHA and guarded against implausibly small artifacts.


## Quick links

- **[Detailed architecture and test notes](docs/RAK_REMOTE_DFU.md)**
- **[Scout source](src/experimental/rak4631_dfu_scout.cpp)**
- **[Target DFU + HOBO source](src/modules/Telemetry/HOBOMX2001MX2201MX2203/HOBOMX2001MX2201MX2203Telemetry.cpp)**
- **[PC → Scout uploader](tools/rak_dfu_serial_upload.py)**
- **[Scout build workflow](.github/workflows/build_rak_dfu_scout.yml)**
- **[Target build workflow](.github/workflows/build_rak_ble_dfu_target.yml)**
- **[RAK4631 PlatformIO environments](variants/nrf52840/rak4631/platformio.ini)**
- **[Scout Actions runs](https://github.com/canyoncountryadventure/firmware/actions/workflows/build_rak_dfu_scout.yml?query=branch%3Aesp-32-testing)**
- **[Target Actions runs](https://github.com/canyoncountryadventure/firmware/actions/workflows/build_rak_ble_dfu_target.yml?query=branch%3Aesp-32-testing)**
- **[Production field-firmware branch](https://github.com/canyoncountryadventure/firmware/tree/field-self-recovery)**

## What happens on the target

A direct Meshtastic text message:

```text
DFU
```

causes the target to:

1. Save the requesting node ID and channel to persistent storage.
2. Reply that BLE OTA DFU is armed.
3. Set nRF52 `GPREGRET = 0xA8`.
4. Reset into the Adafruit `AdaDFU` BLE bootloader.
5. Accept an **application-only** Legacy DFU update.
6. Validate and activate the image.
7. Reboot into Meshtastic.
8. Compare the running firmware's `APP_VERSION` (Meshtastic version + 7-character Git SHA) with the build saved before DFU.
9. If the build changed, send the saved requester:

```text
UPDATE SUCCESS
RAK4631 booted new firmware after BLE DFU
Old: 2.7.26.<oldsha>
New: 2.7.26.<newsha>
```

If the old application simply resumes without a different build being installed, the target instead sends:

```text
DFU NOT CONFIRMED
Previous firmware resumed
Build: 2.7.26.<sha>
```

The pending marker is removed only after one of those result messages is successfully queued.

## Phase 2 bench architecture

The current proven transfer path is:

```text
Meshtastic sender
      |
      | LoRa: "DFU"
      v
Target RAK4631
      |
      | reboots as AdaDFU
      v
Scout RAK4631 <--- USB ---> PC
      |
      | BLE Legacy DFU
      v
Target RAK4631
```

The PC currently supplies the OTA ZIP to the Scout over USB serial. The Scout extracts nothing itself; the Python helper reads the OTA ZIP and streams the target `.dat` and `.bin` as requested.

### Final intended architecture

```text
Phone / controller
      |
      | Meshtastic
      v
Scout RAK4631
      |
      | LoRa: DFU command
      v
Target RAK4631
      |
      | AdaDFU BLE
      v
Scout RAK4631
      |
      | BLE firmware transfer
      v
Target reboots
      |
      | LoRa: UPDATE SUCCESS
      v
Scout / controller
```

That final design does **not require an ESP32**.

## Phase 2 files

The Scout build produces:

- `RAK4631-DFU-Scout-Phase2-Serial-Bridge.uf2`
- `rak_dfu_serial_upload.py`

The target build produces:

- `RAK4631-Meshtastic-HOBO-BLE-DFU-Target.uf2`
- `RAK4631-Meshtastic-HOBO-BLE-DFU-Target-OTA.zip`

Download the newest files from the corresponding Actions run links above.

## Phase 2 usage

1. Flash the latest Scout UF2 onto the Scout RAK4631.
2. Send `DFU` directly to the target over Meshtastic.
3. Confirm the target begins advertising as `AdaDFU`.
4. Connect the Scout to the PC by USB.
5. Run:

```powershell
python .\tools\rak_dfu_serial_upload.py COM45 .\RAK4631-Meshtastic-HOBO-BLE-DFU-Target-OTA.zip
```

Replace `COM45` with the Scout's COM port.

A successful transfer ends with:

```text
PROGRESS 100%
RECEIVE_FW status=0x1
VALIDATE status=0x1
ACTIVATE sent
DFU SUCCESS: target accepted image and rebooted
```

With the current target source, a successful post-update application boot then queues the Meshtastic confirmation back to the node that originally sent `DFU`.

## Safety / recovery

- Phase 2 writes the **application image only**.
- It does not intentionally replace the SoftDevice or bootloader.
- An interrupted application transfer can leave the application invalid.
- The Adafruit/RAK bootloader should remain available for USB UF2 recovery.
- Keep a known-good target UF2 available during development.
- Do not remove power during VALIDATE / ACTIVATE.

## Important implementation detail

The RAK4631 bootloader observed in testing does **not** advertise Nordic Secure DFU `0xFE59`. It advertises:

```text
Name: AdaDFU
Service: 00001530-1212-EFDE-1523-785FEABCD123
Control Point: 00001531-1212-EFDE-1523-785FEABCD123
Packet: 00001532-1212-EFDE-1523-785FEABCD123
```

The Scout supports the actual Legacy DFU path used by this target.

## Repository scope

This repository is a Meshtastic firmware fork and therefore still contains the normal Meshtastic source tree. Files specific to this remote-DFU project are concentrated in the links above. Production environmental-monitoring binaries remain on the **[`field-self-recovery` branch](https://github.com/canyoncountryadventure/firmware/tree/field-self-recovery)**.
