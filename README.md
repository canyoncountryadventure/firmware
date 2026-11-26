# RAK Water Distance + HOBO V8

**RAK4631 + RAK19007 | Unified firmware | November 25, 2026**

V8 combines Meshtastic, HOBO Bluetooth logger polling, and water-distance measurements in a **single RAK firmware image**. It supports the two original ultrasonic sensors plus the new **DYP-A08 UART Controlled** unit. It is designed for your new two-station DYP bench test, while the known-working [V7](https://github.com/canyoncountryadventure/firmware/tree/RAK-Water-Distance-HOBO-v7) remains untouched for the original deployed SEN0313 and MB7388 stations.

> **Release status:** Software regression tests and the unified RAK build passed. The DYP-A08 hardware has **not yet been validated on a real sensor**. The marking `A0811AT67U0141` does not independently establish that the purchased unit is the UART Controlled variant.

## 1. Download the firmware

These links point to **ordinary files committed to the repository**, not expiring GitHub Actions artifacts. The automatic release job publishes these after successfully verifying the build.

| What you need | Direct download |
| --- | --- |
| **RAK4631 USB firmware (UF2)** | **[Download V8 unified UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v8/downloads/RAK-HOBO-v8/field-rak-water-hobo-v8-unified.uf2)** |
| **RAK4631 Bluetooth update (OTA ZIP)** | **[Download V8 unified OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v8/downloads/RAK-HOBO-v8/field-rak-water-hobo-v8-unified-OTA.zip)** |
| Checksums and dated copies | [Browse firmware downloads](https://github.com/canyoncountryadventure/firmware/tree/RAK-Water-Distance-HOBO-v8/downloads/RAK-HOBO-v8) |
| Build logs and temporary ZIP package | [GitHub Actions](https://github.com/canyoncountryadventure/firmware/actions/workflows/build_rak_water_v8.yml) |

**If a direct file link returns 404**, the publishing step has not completed successfully. Check the build status before flashing; the link must resolve to the actual file. The dated UF2/OTA copies in the same directory are retained for identifying precisely what was installed.

**Not interchangeable:** the `.uf2` goes to the RAK bootloader drive using USB. The `-OTA.zip` goes to the BLE DFU updater (for example nRF Connect). Do not flash the separate Scout/drone image onto a field station.

## 2. Which sensors and wiring?

| Sensor | Command | UART data format | Power |
| --- | --- | --- | --- |
| **DFRobot SEN0313 / A01NYUB** | `WATER SENSOR SEN0313` | Automatic 9600 baud; binary `FF, high, low, checksum` | **Pololu 2810**, RAK19007 IO1/P0.17 controls power; 8-second settling |
| **MaxBotix MB7388** | `WATER SENSOR MB7388` | Automatic 9600 baud; ASCII `R####\r` | **Pololu 2810**, RAK19007 IO1/P0.17 controls power; 8-second settling |
| **DYP-A08 UART Controlled** | `WATER SENSOR DYP-A08` | Trigger on RAK UART TX, 9600-baud binary result on RAK UART RX | **Direct regulated 3.3 V**, **no Pololu** |

### DYP-A08: four-wire hookup (new test stations)

| DYP signal | Connect to RAK19007 | Purpose |
| --- | --- | --- |
| **VCC** | **3V3** | Constant regulated power |
| **GND** | **GND** | Common ground |
| **TX** | **RX1** | Sensor sends distance to radio |
| **RX / trigger** | **TX1** | Radio initiates a reading |

**Wire colors need verification.** Your sensor has **red, black, white, and yellow** conductors, but the product/lot marking alone does not prove which of white and yellow is TX versus RX, or conclusively prove the output mode. Verify its wiring legend before energizing it.

**How controlled mode works:** The radio sends one `0x00` byte at 9600 baud on TX1, producing an approximately 0.94-ms low trigger. It then listens for the 4-byte `0xFF, high, low, checksum` distance packet. Software verifies the checksum and an expected 250–8000 mm range, and schedules triggers 200 ms apart. The DYP remains powered at 3.3 V between reads; it is **not** connected through the 2810 switch. A sensor sold as `UART Auto` is **not interchangeable** with this controlled-mode driver.

The original two sensors keep their existing power and wire arrangements. V8 adds DYP support without requiring you to rewire the working V7 installations.

## 3. Flash a RAK4631

### Option A: USB UF2

1. Download the **V8 unified UF2** above to your computer.
2. Connect the **RAK4631 on the RAK19007** by USB.
3. Enter the UF2 bootloader with the normal quick double-reset action (the RAK bootloader drive should appear).
4. Copy the `.uf2` to that drive. Wait for the bootloader to reboot.
5. Pair through Meshtastic and DM the node `VERSION`, then `WATER SENSOR` to verify what actually booted.

### Option B: Bluetooth DFU

1. Download the **V8 unified OTA ZIP**, not the UF2.
2. Connect to the station with the supported Nordic BLE DFU update tool.
3. Select the firmware ZIP, complete the DFU, reconnect, and DM `VERSION`.
4. Leave the station's existing identity and private channel settings alone. Normal application-only updates are intended to preserve them; still verify the radio rejoins your mesh.

**Do not erase settings, factory-reset, or send `LAST RESORT` for a routine update.** `LAST RESORT` is destructive and can strand the station outside your private mesh.

## 4. First-time setup on the two new DYP nodes

Send each command as a **direct message to that station** from a functioning Meshtastic node. These are water commands; bare `READ` means **HOBO**, not water distance.

```text
VERSION
WATER SENSOR DYP-A08
WATER INTERVAL 15M
WATER SAMPLES 5
WATER SENSOR
WATER READ
WATER STATUS
WATER TELEMETRY NOW
```

**What those settings do:**

- `WATER SENSOR DYP-A08`: select and persist controlled UART operation. This setting survives a reboot.
- `WATER INTERVAL 15M`: transmit a distance measurement every **15 minutes** (change later if desired).
- `WATER SAMPLES 5`: require **five fresh valid readings** per report; use the **median**, with up to ten attempts.
- `WATER READ`: take a fresh measurement and DM the distance, and stage if calibrated.
- `WATER STATUS`: give sensor readiness, selected type, calibration, report interval, last read and error counts.
- `WATER TELEMETRY NOW`: queue an immediate distance telemetry measurement.

**Defaults before configuration:** one-hour reporting, ten samples, and the V7 default sensor type. New DYP stations need explicit `WATER SENSOR DYP-A08`. An uncalibrated station can report raw distance; **water stage in feet requires a known-stage calibration**.

### Calibrate water stage

Establish a real reference water stage at the same instant the ultrasonic reading is taken.

```text
WATER CAL STATUS
WATER CAL STAGE 1.42FT
WATER VERIFY
WATER CAL STATUS
```

`1.42FT` is **only an example**. Replace it with your measured staff-gage stage. The radio saves the distance-to-stage reference and automatically locks it. Further changes require `WATER CAL UNLOCK CONFIRM` or a deliberate `WATER CAL RESET CONFIRM`. **Changing a sensor model may invalidate the calibration.** Do not change a deployed node's sensor selection casually.

## 5. Operations and DM command reference

### Water / ultrasonic

| Command | What it does |
| --- | --- |
| `WATER HELP` | Shows available water commands |
| `WATER SENSOR` | Display configured/active sensor and interface |
| `WATER SENSOR DYP-A08` | Select DYP UART Controlled |
| `WATER SENSOR SEN0313` | Select the original DFRobot A01NYUB |
| `WATER SENSOR MB7388` | Select MaxBotix MB7388 |
| `WATER SENSOR AUTO` | Probe original supported sensors; **does not auto-detect controlled DYP** |
| `WATER READ` | New burst measurement with median distance and optional stage |
| `WATER RAW` | New raw-distance measurement |
| `WATER STATUS` | Readiness, interval, calibration, success/error counters |
| `WATER VERIFY` | New burst measurement, raw range/spread, and computed stage |
| `WATER TELEMETRY NOW` | Immediately schedule and transmit distance packet |
| `WATER INTERVAL 1H` | Save schedule (examples: `15M`, `1H`, `6H`) |
| `WATER SAMPLES 5` or `SAMPLES 5` | Save count, valid range 3–20 |
| `WATER CAL STATUS` | Display saved stage reference and lock |
| `WATER CAL STAGE 1.42FT` | Calibrate against a measured stage; locks calibration |
| `WATER CAL UNLOCK CONFIRM` | Unlock for recalibration |
| `WATER CAL RESET CONFIRM` | Clear only water calibration |
| `WATER SENSOR RESTART` or `SENSOR RESTART` | Reinitialize the water driver, preserving configuration |
| `WATER RESET WATER CONFIRM` | Reset **water settings only**, not mesh identity/channels |

### HOBO and radio diagnostics

| Command | Purpose |
| --- | --- |
| `READ` | HOBO logger read, **not** water sensor |
| `LOGGER` | HOBO identification / state |
| `LOCK`, `LOCK AA:BB:CC:DD:EE:FF`, `UNLOCK` | HOBO target assignment |
| `HOBO RESTART` | Restart HOBO BLE recovery without resetting water calibration |
| `HOBO BLE ON`, `HOBO BLE OFF` | Operator control of HOBO BLE |
| `VERSION` | Verify actual firmware label and platform |
| `BLE`, `HEALTH`, `STATUS` | Radio / BLE health |
| `BATTERY`, `BATTERY HISTORY` | Battery now and tracked values since boot |
| `DIAG`, `STATS`, `WATCHDOG` | Recovery breadcrumbs, radio status and watchdog |
| `HUB`, `HUB AUTO` | Telemetry destination / normal broadcast |
| `HUB SET !1234ABCD` | Optional node-specific telemetry destination |
| `REBOOT` | watchdog-origin hardware reset while preserving configuration |
| `SOFT REBOOT`, `RADIO RESET` | Software reset of radio CPU and associated state |
| `LAST RESORT` | **Destructive** factory reset; clears identity, channels/bonds and calibration |

HOBO STATUS is polled about every **30 seconds** and new logger records drive normal HOBO transmission. The distance-report schedule is separate. **Battery telemetry is also separate**; receiving battery packets alone does not prove HOBO BLE reads or ultrasonic measurements are succeeding.

## 6. Telemetry and backend behavior

**Station → Meshtastic LoRa network → receiving Heltec/gateway → database/dashboard.**

- Distance uses the **same versioned 24-byte private water packet format** as V7. The sensor-type byte distinguishes the new DYP (`6`) from the existing sensor identifiers. The gateway must preserve this field; do not silently relabel a DYP packet as SEN0313 or MB7388.
- Raw distance is expressed in **millimeters**. Calibrated stage is computed from the saved sensor reference and carried separately.
- HOBO MX2001/MX2201/MX2203 polling and native battery telemetry remain distinct from the water-distance packets.
- **Cloudflare versus Neon is a gateway/database integration decision.** This RAK image does not itself connect to either database, nor does this firmware release claim that a Cloudflare migration is complete.
- Meshtastic RF parameters, node identity, channel keys, and chosen telemetry destination remain station configuration, not properties of an ultrasonic driver.

## 7. Troubleshooting

| Symptom | First checks |
| --- | --- |
| `VERSION` does not identify V8 | Wrong image installed, update failed, or paired to another node |
| `WATER SENSOR` shows SEN0313/MB7388 | Explicitly DM `WATER SENSOR DYP-A08` on the DYP test station |
| DYP reads all `TIMEOUT` | Verify it is **UART Controlled**; check 3.3 V, common GND, TX→RX1 and RX→TX1 |
| DYP returns `CHECKSUM` / `BAD_FRAME` | Check TX/RX signal integrity, 9600 baud, sensor output mode, and wiring |
| `OUT_OF_RANGE` | Check sensor-to-water distance, target obstruction/echo and model range |
| `WATER READ` works but stage unavailable | Calibrate a **real known stage**; raw distance is not stage |
| `WATER BUSY` | A sample burst is already running; retry after it completes |
| Radio battery arrives but HOBO temperature does not | Check `LOGGER`, `BLE`, `DIAG`, `HOBO RESTART`; battery packets do not validate BLE |
| Radio responds but dashboard lacks water data | Run `WATER TELEMETRY NOW`, then check reception and decoder/backend ingestion |
| Sensor selection blocked | Calibration is locked; unlock deliberately and recalibrate if changing hardware |
| Download URL gives 404 | V8 publish job did not finish; inspect [build status](https://github.com/canyoncountryadventure/firmware/actions/workflows/build_rak_water_v8.yml) |

**Validation gate before remote installation:** actual hardware should pass repeated reads, reboot, checksum/timeout tests, expected 15-minute reports, and overnight operation. A green CI build confirms firmware compilation and scripted tests, **not field performance**.

## 8. Source, tests and rollback

- [V8 source branch](https://github.com/canyoncountryadventure/firmware/tree/RAK-Water-Distance-HOBO-v8)
- [V8 build workflow](https://github.com/canyoncountryadventure/firmware/blob/RAK-Water-Distance-HOBO-v8/.github/workflows/build_rak_water_v8.yml)
- [DYP protocol simulation test](https://github.com/canyoncountryadventure/firmware/blob/RAK-Water-Distance-HOBO-v8/test/test_hobo_v8/test_dyp_a08.py)
- [V8 sensor-preservation test](https://github.com/canyoncountryadventure/firmware/blob/RAK-Water-Distance-HOBO-v8/test/test_hobo_v8/test_preservation_v8.py)
- [V7 working release and historical notes](https://github.com/canyoncountryadventure/firmware/tree/RAK-Water-Distance-HOBO-v7)

**Rollback:** use the known-working V7 image for the two original sensor stations. V7 was not modified for this work. The new DYP stations need V8 for controlled-mode triggering.
