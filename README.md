> **CURRENT BRANCH: RAK-Soil-Moisture-HOBO-v3 — HOBO V3.** Every firmware file linked in the **V3 downloads** table below has `-v3` in its filename. The unchanged `Remote-Drone-Flashing-v2` branch hosts the **V3** compressed Scout files; its branch name is not the firmware image version. The rest of this document describes this V3 configuration (including inherited V2 safeguards).

## V3 downloads — use the file for this exact station

| File | Purpose | Link |
|---|---|---|
| `RAK-Soil-Moisture-HOBO-v3.uf2` | **V3 field radio** — normal USB UF2, for the RAK4631 node | [Download V3 target UF2](https://github.com/canyoncountryadventure/firmware/raw/refs/heads/field-self-recovery/downloads/RAK-Soil-Moisture-HOBO-v3.uf2) |
| `RAK-Soil-Moisture-HOBO-v3-OTA.zip` | **V3 field radio** — BLE OTA update package (not a UF2) | [Download V3 target OTA ZIP](https://github.com/canyoncountryadventure/firmware/raw/refs/heads/field-self-recovery/downloads/RAK-Soil-Moisture-HOBO-v3-OTA.zip) |
| `Drone-RAK-Soil-Moisture-HOBO-v3.uf2` | **V3 drone Scout** — flash ONLY onto the *separate RAK4631 carried by the drone*, never onto the field node | [Download matching V3 drone Scout UF2](https://github.com/canyoncountryadventure/firmware/raw/refs/heads/Remote-Drone-Flashing-v2/downloads/Drone-RAK-Soil-Moisture-HOBO-v3.uf2) |
| `RAK-Soil-Moisture-HOBO-v3-BUILD.txt` | Target build commit and checksums | [View V3 build manifest](https://github.com/canyoncountryadventure/firmware/blob/field-self-recovery/downloads/RAK-Soil-Moisture-HOBO-v3-BUILD.txt) |
| `Drone-RAK-Soil-Moisture-HOBO-v3.uf2.txt` | Scout's embedded target branch, target commit, and checksum | [View V3 Scout manifest](https://github.com/canyoncountryadventure/firmware/blob/Remote-Drone-Flashing-v2/downloads/Drone-RAK-Soil-Moisture-HOBO-v3.uf2.txt) |

**Before a remote update:** compare the target commit in the Scout manifest with the target build manifest. If these differ, use a fresh matching Scout image; do not assume different builds are paired. For USB updates, use the normal target UF2, **not** the `Drone-` UF2.

**V3 operating behavior:** `STATUS` checks the HOBO logger write pointer on a **30-second** healthy-link cadence. The HOBO's internal logging interval is unchanged; automatic mesh telemetry is sent only on a confirmed new record. A manual `READ` remains independent. V3 inherits non-destructive field recovery and the target's existing `DFU` command.

**Source:** [RAK-Soil-Moisture-HOBO-v3](https://github.com/canyoncountryadventure/firmware/tree/RAK-Soil-Moisture-HOBO-v3) · **V3 target build:** [GitHub Actions](https://github.com/canyoncountryadventure/firmware/actions/workflows/build_hobo_v3.yml?query=branch%3ARAK-Soil-Moisture-HOBO-v3) · **V3 matching Scout build:** [GitHub Actions](https://github.com/canyoncountryadventure/firmware/actions/workflows/build_hobo_v3_drone_catalog.yml).

---

> **Field-Recovery v2:** hardened SX1262 recovery, dual nRF52 watchdog channels, missed RX/TX IRQ polling, guarded AGC calibration, radio rail power-cycle recovery, flash-safe reboot, 12-hour burn-in reboot, and increased nRF52/BLE task stacks. Sensor behavior from RAK-Soil-Moisture-HOBO-v1 is retained.

# RAK Soil Moisture + HOBO v2

Production field firmware for **RAK4631 + RAK19007** stations using a **DFRobot SEN0308 waterproof capacitive soil-moisture sensor** and optional **Onset HOBO MX-series BLE temperature/water-level loggers**.

**Branch:** `RAK-Soil-Moisture-HOBO-v3`

**Supported HOBO loggers:** MX2001, MX2201, MX2203.

This build combines the proven HOBO next-record telemetry path, SEN0308 analog soil moisture, direct-message diagnostics, raw-data preservation, non-destructive self-recovery, and nRF52840 watchdog protection for unattended stations.

## Download and project links

| Item | Link |
|---|---|
| **USB firmware (UF2)** | **[Download RAK-Soil-Moisture-HOBO-v3.uf2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-Soil-Moisture-HOBO-v3.uf2)** |
| **BLE DFU firmware (ZIP)** | **[Download RAK-Soil-Moisture-HOBO-v3-OTA.zip](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-Soil-Moisture-HOBO-v3-OTA.zip)** |
| Source branch | [RAK-Soil-Moisture-HOBO-v3](https://github.com/canyoncountryadventure/firmware/tree/RAK-Soil-Moisture-HOBO-v3) |
| Build workflow runs | [Build RAK Soil Moisture HOBO v2](https://github.com/canyoncountryadventure/firmware/actions/workflows/build_soil_moisture_hobo_rak4631.yml?query=branch%3ARAK-Soil-Moisture-HOBO-v3) |
| Workflow source | [build_soil_moisture_hobo_rak4631.yml](.github/workflows/build_soil_moisture_hobo_rak4631.yml) |
| Soil module source | [SEN0308SoilMoisture.cpp](src/modules/Telemetry/SoilMoisture/SEN0308SoilMoisture.cpp) |

The UF2 and OTA ZIP are built from this branch and then published as stable downloads on the repository's default `field-self-recovery` branch.

## Fast start

1. Seat the RAK4631 firmly in the RAK19007 and attach the LoRa and BLE antennas.
2. Wire the SEN0308 exactly as shown below. Use `VDD`, **not `VBAT`**.
3. Flash the UF2 over USB or the unopened OTA ZIP through BLE DFU.
4. Configure the normal Meshtastic region, LoRa preset, frequency slot, channel and keys.
5. Direct-message `SOIL READ`, then `SOIL STATUS` and `SOIL TX`.
6. If using a HOBO logger, run `LOGGER`, `LOCK`, reboot, and run `LOGGER` again.
7. Confirm at least one automatic soil packet and one real next-record HOBO packet reach the intended receiver/dashboard before deployment.

## Hardware

### Soil sensor wiring — RAK19007

| SEN0308 lead | RAK19007 | Purpose |
|---|---|---|
| Red | `VDD` | 3.3 V power |
| Yellow | `AIN1` | Analog soil signal |
| Black | `GND` | Ground |
| Black | `GND` | Ground; both black leads are common |

RAK19007 `AIN1` maps to RAK4631 **A1 / P0.31 / AIN7**.

The RAK variant enables the WisBlock 3.3-V rail during startup, so the SEN0308 can be powered directly from `VDD`.

Do not connect the SEN0308 red lead to `BAT`/`VBAT`. `BAT` is the raw single-cell LiPo rail; `VDD` is the regulated 3.3-V sensor rail.

## Flashing

### USB — recommended and recovery method

1. Download the **UF2** above.
2. Connect USB-C to the RAK19007.
3. Double-press reset to expose the RAK4631 bootloader drive.
4. Copy the UF2 onto that drive.
5. Let the drive disconnect and the application reboot.

### Android BLE DFU

1. Download the **OTA ZIP** above to the phone. **Do not unzip it.**
2. Open Nordic **nRF Connect** and connect to the RAK4631.
3. Tap **DFU** in the upper-right corner.
4. Choose **Distribution packet (ZIP)** and select `RAK-Soil-Moisture-HOBO-v3-OTA.zip`.
5. Let validation, activation and reboot finish, then reconnect in Meshtastic and send `VERSION` and `SOIL STATUS`.

The UF2 is USB-only. The OTA ZIP is BLE-only. If a legacy BLE update reaches 100% but stalls during validation/activation, stop retrying and recover with the UF2 over USB. Routine updates should not use a factory-erase image.

## SEN0308 field calibration

Calibration was measured on this exact SEN0308 + RAK4631/RAK19007 setup at 10-bit ADC resolution.

| Condition | Measured ADC10 |
|---|---:|
| Dry air | ~634 |
| Super-dry soil | ~580 |
| Slightly moist soil | ~294 |
| Wet/saturated soil | ~0 |
| Water | ~0 |

The field scale intentionally uses **soil endpoints**, not dry air:

- **0% moisture = ADC10 580**
- **100% moisture = ADC10 0**
- Higher ADC = drier
- Lower ADC = wetter

Conversion:

```text
moisture_percent = clamp((580 - ADC10) / 580 * 100, 0, 100)
```

The firmware averages **20 analog samples** separated by **10 ms** before calculating moisture.

Because saturated soil and open water both drive this sensor close to zero, the sensor should be treated as a soil-moisture indicator, not as a way to distinguish saturated soil from standing water.

The reported percentage is a field-calibrated moisture index, **not laboratory volumetric water content**. Soil texture, salinity, temperature, installation contact and sensor-to-sensor variation can shift the response. Preserve and use `ADC10` when comparing sites or refining calibration.

## Soil telemetry

The node automatically samples and transmits soil moisture **once per hour** after the initial startup delay.

Each automatic soil reading produces:

1. A normal Meshtastic `TELEMETRY_APP` environment packet using the standard `soil_moisture` field, so ordinary Meshtastic telemetry consumers can display the calculated percentage.
2. A compact `PRIVATE_APP` raw packet so the original ADC measurement is retained for future recalibration.

Raw packet format, version 1:

| Byte | Meaning |
|---:|---|
| 0 | `S` |
| 1 | `M` |
| 2 | Packet version = `1` |
| 3 | Moisture percent, 0–100 |
| 4–5 | ADC10 raw value, little-endian |
| 6–7 | Soil telemetry sequence, little-endian |

Keeping the raw ADC value means the percentage calibration can be changed later without losing the original sensor response.

Current fixed behavior:

- Automatic soil interval: **1 hour**.
- Automatic soil output channel: **Meshtastic channel 0**.
- Calibration endpoints: **ADC10 580 = 0%**, **ADC10 0 = 100%**.
- `SOIL CAL` reports the endpoints; it does not modify them.
- `SOIL READ` replies only by DM and does not create an automatic telemetry event.
- `SOIL TX` forces both the standard telemetry packet and the raw packet.
- The raw sequence counter restarts after a reboot.
- Normal Meshtastic apps can consume the standard `soil_moisture` field; the compact `PRIVATE_APP` packet requires a custom decoder/ingestion path.

## HOBO automatic telemetry

The HOBO path is inherited from the field-proven RAK HOBO Safe firmware.

- Uses HOBO `STATUS` and the logger write pointer rather than a blind radio timer.
- Uses the proven `NEWREAD64` record path.
- Detects the logger's actual logging interval.
- Automatically transmits only after a real new HOBO record is written.
- Manual `READ` does **not** consume or advance the automatic write-pointer baseline.
- `LOCK` persists the intended logger assignment across reboot.
- Supports MX2001, MX2201 and MX2203.
- MX2201/MX2203 temperature is emitted as normal Meshtastic environmental temperature telemetry.
- MX2001 retains the existing combined water-level/temperature private-packet behavior.

## Direct-message commands

Send commands as a direct Meshtastic text message to the node. Commands are case-insensitive; a leading `/` is optional where supported.

### Soil commands

| Command | What it does |
|---|---|
| `SOIL` | Takes a fresh soil reading and replies with moisture % and raw ADC10. |
| `SOIL READ` | Same as `SOIL`. DM-only; does not create an automatic telemetry event. |
| `SOIL STATUS` | Shows the last reading, automatic interval, input pin and calibration endpoints. |
| `SOIL TX` | Takes a fresh reading, broadcasts standard soil telemetry + raw packet, and reports TX status by DM. |
| `SOIL CAL` | Shows the active dry/wet calibration and sensor direction. |
| `SOIL HELP` | Lists soil commands. |

### HOBO commands

| Command | What it does |
|---|---|
| `LOGGER` | Shows HOBO model, MAC, BLE RSSI, logging interval, target and lock state. |
| `READ` | Requests a fresh HOBO measurement without consuming the automatic record pointer. |
| `LOCK` | Saves the currently identified HOBO as this station's logger. |
| `UNLOCK` | Clears the saved logger assignment and resumes discovery. |
| `AUTO` | Confirms pointer-gated next-record automatic HOBO operation. |

### Node / recovery commands

| Command | What it does |
|---|---|
| `HELP` | Shows the existing HOBO/recovery command summary. Use `SOIL HELP` for soil commands. |
| `STATUS` / `HEALTH` | Compact node and self-recovery health. |
| `POWER` / `BATTERY` | Battery voltage, percentage, battery-present and charging state. |
| `BLE` | Shows central-link count, scanner state, and confirms that the HOBO state machine owns scanner/link lifecycle. |
| `WATCHDOG` | Shows the 90-second core watchdog, independent field-health watchdog channel, and sleep/halt behavior. |
| `STATS` | Recovery counts and boot/reset information. |
| `NODES` | Meshtastic NodeDB count. |
| `UPTIME` | Node uptime. |
| `VERSION` | Self-recovery firmware/platform identity. |
| `PING` / `WAKE` | End-to-end DM/liveness check. |
| `SCAN` | Legacy diagnostic command; v2 reports that BLE recovery is automatic and does not manipulate the scanner. |
| `RECONNECT` | Legacy diagnostic command; v2 does not force a link rebuild because scanner/link lifecycle is owned by the HOBO state machine. |
| `RECOVER` / `REBOOT` | Replies, then performs a safe non-destructive reboot. |

## Watchdog and unattended recovery

This branch uses the **Field-Recovery v2 nRF52840 recovery model**.

- The normal main-loop hardware watchdog is **90 seconds**.
- A second, independent field-health watchdog channel is allocated from the same nRF52840 WDT and can be deliberately starved when radio/BLE recovery is exhausted.
- Both watchdog channels run during CPU sleep/halt in these always-on environmental builds.
- A BLE connection attempt is cancelled after **30 seconds** if it does not complete.
- Three consecutive `STATUS` failures/timeouts or three automatic `NEWREAD64` timeouts rebuild the BLE link.
- Five exhausted BLE recovery cycles trip the independent field watchdog and force a hardware reset.
- Repeated unrecoverable SX1262 state loss also escalates to the field watchdog.
- A **12-hour preventive reboot** remains enabled during v2 burn-in as an independent final fallback.
- `RECOVER` / `REBOOT` schedules the same flash-safe, non-destructive whole-node reboot remotely.

Routine recovery must preserve:

- Meshtastic node identity
- LoRa/channel settings and PSKs
- NodeDB
- saved HOBO logger assignment

Factory erase is **not** part of normal recovery.

## Recommended bench / field validation

After flashing:

```text
PING
WATCHDOG
POWER
SOIL READ
SOIL STATUS
SOIL TX
LOGGER
READ
LOCK
LOGGER
AUTO
BLE
STATUS
```

For soil validation, compare `SOIL READ` against known conditions. The original bench points were approximately 580 in super-dry soil, 294 in slightly moist soil, and 0 in saturated soil.

For HOBO validation, lock the correct logger, reboot, confirm `LOGGER` restores the assignment, then wait through at least one real logger interval and verify automatic telemetry occurs only after the next logger record.

### Pass criteria

| Check | Required result |
|---|---|
| `SOIL READ` in dry soil | Plausible low percentage and stable ADC10 near the dry calibration range. |
| `SOIL READ` in moist soil | Lower ADC10 and higher percentage than the dry test. |
| `SOIL TX` | DM reports `telemetry=OK raw=OK`; receiver/dashboard gets the standard soil packet. |
| Reboot | Meshtastic identity, channel configuration and locked HOBO assignment remain intact. |
| One-hour run | A new automatic soil telemetry event is received. |
| HOBO interval | A HOBO packet is generated only after the logger write pointer advances. |
| `WATCHDOG` after 30 seconds | Reports `core=90s`, the field channel armed, and run-in-sleep enabled. |

## Troubleshooting

### Three flashes, pause, repeat

This is the nRF52840 low-voltage safe-boot loop. The firmware measured the regulated VDD rail below **2.7 V** and stopped before Meshtastic, HOBO and the USB console were initialized.

1. Disconnect the battery, solar input, SEN0308 and other accessories.
2. Leave the LoRa antenna attached.
3. Power the RAK19007 from a known-good 5-V USB-C source and press reset once.
4. If the pattern continues, remove power, firmly reseat the RAK4631, and retry.

A blank serial console during this exact three-flash pattern is expected because console initialization occurs after the power-safety check.

### Soil always reads 0% or 100%

- Confirm red is on `VDD`, yellow is on `AIN1`, and both black wires are on `GND`.
- Verify the signal is not on `VBAT`, `IO1`, `IO2`, `RX1` or `TX1`.
- Use `SOIL READ` repeatedly while moving between dry and moist material and compare the raw ADC10 value, not only the percentage.
- ADC10 near 0 in both saturated soil and water is expected for this calibration.

## Build target

```text
rak4631
```

The branch uses the same validated Meshtastic baseline as the RAK HOBO Safe build. The GitHub Actions workflow builds the RAK4631 release artifacts and publishes a permanent UF2/OTA copy for this branch.

Use the normal UF2 or BLE DFU package for routine updates. Do not use factory-erase firmware for ordinary upgrades.

## Scope and deployment status

- Hardware target: **RAK4631 + RAK19007 only**.
- Soil sensor: **DFRobot SEN0308 analog waterproof capacitive sensor**.
- HOBO support: **MX2001, MX2201 and MX2203**.
- Soil sampling, fixed calibration, standard telemetry, raw telemetry and DM commands are implemented in this branch.
- The exact SEN0308 calibration points above came from the bench setup documented for this project; field soil should still be checked against gravimetric or site-specific reference measurements when quantitative soil-water content matters.
