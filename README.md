# RAK Water Distance + HOBO V5

Custom Meshtastic firmware for **RAK4631**, combining a UART water-distance sensor with a Bluetooth HOBO logger. Two builds are available: **SEN0313 / A01NYUB** for Pack Creek and **MaxBotix MB7388** for Cottonwood. Site names are deployment examples; neither build is tied to a node ID or site.

HOBO support: **MX2001** water level and temperature, **MX2201** temperature, and **MX2203** temperature.

## Download V5

Download the build matching your distance sensor. **Field UF2 is for USB installation; OTA ZIP is for Nordic BLE DFU; Drone Scout UF2 goes on the separate RAK4631 carried by the drone.**

| Sensor / deployment | Field USB firmware | Field BLE DFU firmware | Drone Scout firmware | Checksums / Scout manifest |
| --- | --- | --- | --- | --- |
| SEN0313 / A01NYUB — Pack Creek | [Download SEN0313 UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-Water-HOBO-v5/RAK-Water-HOBO-v5-SEN0313.uf2) | [Download SEN0313 OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-Water-HOBO-v5/RAK-Water-HOBO-v5-SEN0313-OTA.zip) | [Download SEN0313 drone Scout UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Remote-Drone-Flashing-v2/downloads/RAK-Water-HOBO-v5/Drone-RAK-Water-HOBO-v5-SEN0313.uf2) | [Field SHA-256](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-Water-HOBO-v5/SHA256-SEN0313.txt) · [Scout manifest](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Remote-Drone-Flashing-v2/downloads/RAK-Water-HOBO-v5/Drone-RAK-Water-HOBO-v5-SEN0313.uf2.txt) |
| MB7388 — Cottonwood | [Download MB7388 UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-Water-HOBO-v5/RAK-Water-HOBO-v5-MB7388.uf2) | [Download MB7388 OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-Water-HOBO-v5/RAK-Water-HOBO-v5-MB7388-OTA.zip) | [Download MB7388 drone Scout UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Remote-Drone-Flashing-v2/downloads/RAK-Water-HOBO-v5/Drone-RAK-Water-HOBO-v5-MB7388.uf2) | [Field SHA-256](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-Water-HOBO-v5/SHA256-MB7388.txt) · [Scout manifest](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Remote-Drone-Flashing-v2/downloads/RAK-Water-HOBO-v5/Drone-RAK-Water-HOBO-v5-MB7388.uf2.txt) |

The links above follow the latest published V5 build. At this documentation update, the verified firmware is **Meshtastic 2.7.26, source commit `659d1f4ca0d9e6f4e83c4e3733ba799a062a28b6`**, built October 12, 2026 UTC. A documentation commit does not change the firmware in these downloads.

- [Successful build and tests](https://github.com/canyoncountryadventure/firmware/actions/runs/38201820946)
- [V5 source branch](https://github.com/canyoncountryadventure/firmware/tree/RAK-Water-Distance-HOBO-v5)
- [Build workflow](https://github.com/canyoncountryadventure/firmware/actions/workflows/build_rak_water_v5.yml?query=branch%3ARAK-Water-Distance-HOBO-v5)
- [Immutable copies of this verified build](https://github.com/canyoncountryadventure/firmware/tree/9deed6c8e62eb92776807fbf91e80c36cc2bcf96/downloads/RAK-Water-HOBO-v5)

These downloads are for **RAK4631 / nRF52840**, not Heltec. Each V5 Scout embeds the exact published V5 OTA application for its sensor build. The Scout implementation remains on `Remote-Drone-Flashing-v2`; that branch name identifies the flasher implementation, not the embedded field-firmware generation. **An older V4 Scout installs its embedded V4 firmware; it does not fetch V5 from the internet.**

## Why we updated

Cottonwood's V4 logs showed repeated HOBO service-discovery failures and connection timeouts. They also showed that the node received and decrypted `LOGGER` DMs and transmitted replies. Lack of a visible reply therefore did not prove the radio had stopped receiving.

V4 eventually deliberately stopped feeding the independent field watchdog. The resulting hardware reset recovered the logger in that capture: after reboot it reached the BLE command channel and identified an **MX2001**. This demonstrated that recovery could work, but the process left several failure paths unbounded or uncounted.

The audit found four concrete problems: discovery/setup failures did not count toward BLE recovery, scan recovery relied on an SDK running flag that could remain stale after failure, initialization writes could retry without a deadline, and command replies on this branch did not request acknowledgment. V5 addresses those paths and makes the logger's model and recovery state visible.

**The underlying RF/GATT trigger is still unproven.** The Cottonwood capture does not establish Pack Creek's exact cause; that needs Pack Creek's own log. V5 has passed builds and fault-injection tests, but still needs live deployment validation.

## V4 → V5 differences

V5 is based on [V4 commit `42af584`](https://github.com/canyoncountryadventure/firmware/commit/42af584eaff4f72fdbb99c64ac8ed1339375e78c). The V4 branch is unchanged.

| Area | V4 | V5 |
| --- | --- | --- |
| BLE discovery | Discovery in the connection callback; setup failures could escape the recovery counter | Service discovery, characteristic discovery, and notification setup run in bounded module-task stages; exhausted stages enter recovery |
| Setup deadlines | Initialization writes could retry indefinitely | 45-second GATT setup deadline; 30-second bootstrap deadline through the first successful STATUS baseline |
| Scanner recovery | Relied on cached `isRunning()` state | Checks scan start/resume results; restarts disconnected scanning every 30 seconds even with a stale running flag; 5-second scan retry backoff |
| Acquisition scanning | 10% receive window | 50% receive window while seeking a logger; scanning stops while connecting or connected |
| Bad link cleanup | Incomplete cancellation/disconnect escalation | Checks cancellation; a disconnect stalled for 10 seconds or a failed cancellation escalates to the field watchdog |
| Reset escalation | Five counted BLE recovery failures; discovery failures were not counted | Five recovery failures spanning at least two minutes trigger the independent 90-second watchdog; discovery/setup failures participate |
| Advertisement selection | Could attempt non-connectable HOBO advertisements | Ignores non-connectable advertisements |
| `LOGGER` | MAC-focused response | Verified MX2001/MX2201/MX2203 model, connection/recovery state, lock and last error; persisted historical identity is explicitly labeled |
| Locking | Bare `LOCK` after a verified connection | Also accepts `LOCK AA:BB:CC:DD:EE:FF` before connecting; target persists |
| DM replies | Acknowledgment disabled on this water branch | HOBO, system, and water replies request acknowledgment/retry |
| Sensor builds | Shared sensor configuration | Separate SEN0313 and MB7388 build defaults; saved sensor selection still takes precedence |

### What stays the same

- Existing saved sensor selection, calibration, distance measurement interval, logger lock, node identity, keys, and channels remain compatible with an ordinary application update.
- Distance configuration format and cloud telemetry formats are unchanged.
- The MB7388 cold-start settling fix, existing LoRa recovery, and 12-hour preventive reboot are retained.
- HOBO STATUS is polled every 30 seconds. Automatic telemetry is sent only when the logger has a **new recorded sample**. An initial successful probe does not fabricate an hourly record.
- V5 does not change the logger's configured recording interval. If the MX2001 records hourly, a connected logger may still wait until the next record before new automatic telemetry appears.

## Install and verify

### USB / UF2

1. Download the UF2 matching the distance sensor.
2. Connect the RAK4631 over a USB data cable and double-tap reset to enter its UF2 bootloader.
3. Copy the UF2 onto the bootloader drive. The node reboots after installation.
4. Reconnect in Meshtastic and DM the checks below directly to the node.

Use an ordinary application update to preserve settings. A factory erase/reset is a separate operation and can remove configuration.

### BLE DFU / OTA ZIP

Download the matching **OTA ZIP** and leave it zipped. Use a Nordic DFU client, put the node into DFU mode with the existing `DFU` command, and select the ZIP as the application update package. This is Nordic BLE DFU, not a Meshtastic text-file transfer. USB remains the fallback if the node cannot receive the DFU command.

### Drone flashing — autonomous Scout

Use the matching **Drone Scout UF2** from the download table. The Scout is a **separate RAK4631 carried by the drone**. Flashing a `Drone-*.uf2` onto the field node replaces its normal sensor application with the flasher.

1. Download the SEN0313 or MB7388 Scout and its manifest. Check that the manifest's **Target commit** is `659d1f4ca0d9e6f4e83c4e3733ba799a062a28b6` for this release and that its target OTA checksum matches the selected published field OTA ZIP. Verify the Scout UF2 checksum in the manifest.
2. Install the Scout UF2 onto the separate drone RAK4631 over USB. This Scout carries the compressed application internally; it needs no PC, SD card, internet, or phone connection during the transfer.
3. Power the Scout and position it within BLE range of the intended field radio. From your separate Meshtastic controller, DM **`DFU` to the field radio**. The field radio must already run firmware with our remote DFU hook, such as the existing V4 or V5 target firmware.
4. The field radio stores and verifies the update-result callback marker, then reboots into the AdaDFU bootloader. The Scout detects the Legacy DFU service and automatically transfers its embedded application.
5. The field radio validates and activates the application, reboots into Meshtastic, and queues the update-result DM to the requester. Verify `VERSION`, `LOGGER`, and `WATER STATUS` afterward. A callback is queued over the mesh; reception still depends on the route.

Put only the intended nearby field node into DFU at a time: the Scout detects compatible DFU advertisements rather than selecting a Meshtastic node ID. If the target cannot receive the `DFU` DM, flying the Scout nearby does not itself switch the target into its bootloader.

The drone transfer installs the same application as the matching V5 field OTA ZIP and retains the target's compatible stored settings. A saved sensor selection still overrides the build default. Changing the payload requires flashing the other matching Scout image onto the drone RAK4631; the Scout does not choose SEN0313 versus MB7388 automatically.

The original Scout/target protocol has been demonstrated on hardware. **These exact V5 Scout payloads are build- and package-verified; a physical end-to-end V5 transfer remains to be tested.**

### First checks after flashing

```text
VERSION
LOGGER
READ
WATER STATUS
WATER CAL STATUS
WATCHDOG
```

`VERSION` should identify V5. `LOGGER` should show the actual verified model when identified. `READ` requests a HOBO reading; `WATER READ` requests the distance sensor instead.

**Saved settings override build defaults.** Flashing the MB7388 build onto a node previously configured for SEN0313 does not silently switch its stored sensor selection. Check `WATER SENSOR`; when a change is intended, use one of:

```text
WATER SENSOR MB7388
WATER SENSOR SEN0313
```

Then check `WATER STATUS` and the calibration before relying on reported stage. Preserve an existing valid calibration; changing the sensor or physical installation may require recalibration.

To choose a specific HOBO before it connects:

```text
LOCK AA:BB:CC:DD:EE:FF
LOGGER
```

Replace the example with the intended logger's actual address. Do not guess an address from another site's logger. Bare `LOCK` still locks the currently connected, identified logger; `UNLOCK` removes the restriction.

## What `LOGGER` tells you

An identified logger response includes:

```text
HOBO CONNECTED
Model: MX2001
Target/Candidate: AA:BB:CC:DD:EE:FF
BLE: -73 dBm
Interval: 3600 sec
Lock: ON
Last error: ...
```

When disconnected after a previous identification, it can instead show:

```text
HOBO SCANNING
Model: Not identified
Last verified: MX2001
Last MAC: AA:BB:CC:DD:EE:FF
Lock: ON
Failures: 0
Last error: ...
```

These are example values. `Last verified` is historical, not proof of a current connection. The firmware only reports the current model after reading and validating it. Other states include `CONNECTING`, `IDENTIFYING`, `RECOVERING`, and `RESET PENDING`. The last error is diagnostic history and may remain visible after recovery.

## DM command reference

Send commands as **direct messages to the node**. In the combined build, prefix distance-sensor commands with `WATER`; bare `READ` belongs to HOBO. `HELP` and `WATER HELP` provide the on-device reference.

| HOBO command | Purpose |
| --- | --- |
| `LOGGER` | Model, state, address/last verified identity, lock, and recovery diagnostics |
| `READ` | Request a current HOBO reading in a DM reply |
| `LOCK` | Persist the connected, identified logger as the target |
| `LOCK AA:BB:CC:DD:EE:FF` | Persist an explicit target before connection |
| `UNLOCK` | Remove the logger address restriction |
| `AUTO` | Report automatic polling/new-record transmission behavior |
| `SCAN`, `RECONNECT` | Report state-machine ownership; these do not manually restart the scanner |

| Water command | Purpose |
| --- | --- |
| `WATER`, `WATER HELP` | Command reference |
| `WATER STATUS`, `WATER CHECK`, `WATER INSTALL` | Installation/configuration status |
| `WATER SENSOR` | Show sensor selection |
| `WATER SENSOR MB7388` | Select MaxBotix MB7388 |
| `WATER SENSOR SEN0313`, `WATER SENSOR A01NYUB` | Select SEN0313 / A01NYUB |
| `WATER SENSOR AUTO`, `SEN0590`, `SEN0311`, `A02YYUW` | Other supported selections; each requires the full `WATER SENSOR` prefix |
| `WATER READ`, `WATER RAW`, `WATER VERIFY` | Measurement and sensor diagnostics |
| `WATER INTERVAL 1H` | Example distance measurement interval setting |
| `WATER CAL STATUS` | Show stored calibration |
| `WATER CAL STAGE 1.42FT` | Example: calibrate using a known current stage; replace with the measured value |
| `WATER CAL LOCK` | Lock calibration |
| `WATER CAL UNLOCK`, `WATER CAL UNLOCK CONFIRM` | Unlock calibration with confirmation |
| `WATER CAL RESET`, `WATER CAL RESET CONFIRM` | Clear calibration with confirmation |
| `WATER RESET WATER`, `WATER RESET WATER CONFIRM` | Reset water configuration with confirmation |
| `WATER TELEMETRY NOW` | Request water telemetry now |
| `WATER MODE` | Report operating mode |

| System command | Purpose |
| --- | --- |
| `PING`, `WAKE` | Connectivity check |
| `VERSION`, `UPTIME` | Build and runtime information |
| `STATUS`, `HEALTH` | Node health summary |
| `BLE`, `WATCHDOG` | BLE and watchdog diagnostics |
| `POWER`, `BATTERY` | Power information |
| `STATS`, `NODES` | Radio/node diagnostics |
| `DIAG`, `CRASHLOG` | Saved diagnostics |
| `CLEAR DIAG` | Clear saved diagnostics |
| `RECOVER`, `REBOOT` | Request a software reboot |
| `DFU` | Enter Nordic BLE DFU mode |

## Recovery behavior and tradeoffs

The node keeps seeking a logger. **No advertisements alone do not cause repeated watchdog resets.** When actual connection/setup failures accumulate, recovery can escalate to the independent hardware watchdog. Five failures must span at least two minutes before that escalation; once the watchdog is deliberately starved, reset takes about 90 seconds. These are recovery thresholds, not a guaranteed time to connect.

The 12-hour preventive software reboot is separate from the hardware watchdog path. Both restart firmware, but they have different triggers/reset mechanisms. Neither establishes that an intermittent peripheral, RF, or logger problem has disappeared.

The wider scan window improves acquisition opportunities but increases power use while disconnected. Acknowledged replies add LoRa traffic and still depend on a working mesh route. V5 does not guarantee an immediate connection or immediate cloud update.

## Validation and remaining checks

**Passed:** both RAK4631 target builds, host fault-injection tests, and download packaging checks in [run 38201820946](https://github.com/canyoncountryadventure/firmware/actions/runs/38201820946). Fault-injection tests exercise extracted firmware code for stale scanner state, failed scan starts/cancellation, discovery failure escalation, successful setup, and truthful model reporting.

The published field UF2 files were checked for nRF52840 family ID, block structure, application address bounds, source commit, V5 label, and LOGGER labels. Both UF2 files are 1,495,552 bytes. OTA ZIP integrity and application manifests were also checked.

| Verified build | UF2 SHA-256 |
| --- | --- |
| SEN0313 | `e72aea487939657622371a0072938bf812cfede0e70bb234101bf98bfb93d1a9` |
| MB7388 | `d9979b96f5ad4d57dd5764331a55d52fc94eebeb950b574a906f04ee7e943151` |

These hashes describe source `659d1f4`; use the linked checksum files for a future latest build.

**Still required on hardware:** battery startup without a manual reset, logger absent then returning, disconnect during discovery/reading, DM replies during recovery, saved settings across a watchdog reset, and a 24-hour run with the expected recorded telemetry. Compilation and simulated failures do not replace these checks.

For a failure report, capture serial logs from power-on through recovery and include `VERSION`, `LOGGER`, `WATCHDOG`, `WATER STATUS`, the logger model/address, power source, and whether any reset was manual. A Pack Creek-specific log is needed to diagnose Pack Creek's exact failure.

### V5 drone Scout verification

Both Scout builds and publication passed in [run 38313709229](https://github.com/canyoncountryadventure/firmware/actions/runs/38313709229). The workflow builds both sensor payloads from the already-published OTA ZIPs, pinned by immutable catalog commit and SHA-256. The packaging check verifies nRF52840 UF2 family/address/block structure, the exact embedded DFU init packet, and byte-for-byte LZ4 decompression back to the corresponding V5 application. The build also checks the Scout's streaming decoder against that application.

Each linked Scout manifest records the embedded target source commit, target environment, OTA checksum, Scout implementation commit, UF2 size, and UF2 checksum. A newer README commit is not a different firmware payload. After a firmware change, rebuild the matching Scout before using it to install that new version.

## Developer reference

```sh
python3 test/test_hobo_v5/test_hobo_v5.py
pio run -e rak4631_sen0313_v5
pio run -e rak4631_mb7388_v5
```

Build environments are defined in `variants/nrf52840/rak4631/platformio.ini`. The build workflow verifies target-specific artifacts before publishing downloads. [V5 engineering notes](V5-WATER-HOBO.md) document the audit and compatibility details.
