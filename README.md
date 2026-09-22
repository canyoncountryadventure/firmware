# Meshtastic Field Firmware — HOBO V3 and V2 catalog

## HOBO V3: 30-second STATUS checks

V3 polls the connected HOBO write pointer every 30 seconds when healthy and connected. It queues a fresh live measurement for mesh transmission **only after a new logger write pointer is confirmed**. V3 does not change the HOBO's configured recording interval. Manual `READ` remains independent. Existing V2 logger lock, watchdog/recovery, sensor functions, and drone-DFU target hook are retained. Implausibly rapid repeated automatic transmissions are suppressed relative to the logger interval.

The **USB UF2** belongs on the field radio. The **BLE OTA ZIP** is the application-only update package. The **matched compressed Scout UF2** belongs on the separate RAK4631 drone flasher, never on the target radio. Match each Scout image with the target row and hardware.

| V3 target configuration | Field radio UF2 | Target BLE OTA ZIP | Matched compressed drone Scout UF2 |
|---|---|---|---|
| Canonical RAK4631 | [USB UF2](downloads/Field-Self-Recovery-v3-RAK4631.uf2) | [BLE OTA ZIP](downloads/Field-Self-Recovery-v3-RAK4631-OTA.zip) | [Matched Scout UF2](https://github.com/canyoncountryadventure/firmware/blob/Remote-Drone-Flashing-v2/downloads/Drone-Field-Self-Recovery-v3-RAK4631.uf2) |
| Canonical Seeed XIAO | [USB UF2](downloads/Field-Self-Recovery-v3-Seeed.uf2) | [BLE OTA ZIP](downloads/Field-Self-Recovery-v3-Seeed-OTA.zip) | [Matched Scout UF2](https://github.com/canyoncountryadventure/firmware/blob/Remote-Drone-Flashing-v2/downloads/Drone-Field-Self-Recovery-v3-Seeed.uf2) |
| RAK HOBO Safe | [USB UF2](downloads/RAK-HOBO-Safe-v3.uf2) | [BLE OTA ZIP](downloads/RAK-HOBO-Safe-v3-OTA.zip) | [Matched Scout UF2](https://github.com/canyoncountryadventure/firmware/blob/Remote-Drone-Flashing-v2/downloads/Drone-RAK-HOBO-Safe-v3.uf2) |
| RAK Soil Moisture + HOBO | [USB UF2](downloads/RAK-Soil-Moisture-HOBO-v3.uf2) | [BLE OTA ZIP](downloads/RAK-Soil-Moisture-HOBO-v3-OTA.zip) | [Matched Scout UF2](https://github.com/canyoncountryadventure/firmware/blob/Remote-Drone-Flashing-v2/downloads/Drone-RAK-Soil-Moisture-HOBO-v3.uf2) |
| RAK Water Distance + HOBO | [USB UF2](downloads/RAK-Water-Distance-HOBO-v3.uf2) | [BLE OTA ZIP](downloads/RAK-Water-Distance-HOBO-v3-OTA.zip) | [Matched Scout UF2](https://github.com/canyoncountryadventure/firmware/blob/Remote-Drone-Flashing-v2/downloads/Drone-RAK-Water-Distance-HOBO-v3.uf2) |
| Seeed HOBO Safe | [USB UF2](downloads/Seeed-HOBO-Safe-v3.uf2) | [BLE OTA ZIP](downloads/Seeed-HOBO-Safe-v3-OTA.zip) | [Matched Scout UF2](https://github.com/canyoncountryadventure/firmware/blob/Remote-Drone-Flashing-v2/downloads/Drone-Seeed-HOBO-Safe-v3.uf2) |
| Seeed Water Distance + HOBO | [USB UF2](downloads/Seeed-Water-Distance-HOBO-v3.uf2) | [BLE OTA ZIP](downloads/Seeed-Water-Distance-HOBO-v3-OTA.zip) | [Matched Scout UF2](https://github.com/canyoncountryadventure/firmware/blob/Remote-Drone-Flashing-v2/downloads/Drone-Seeed-Water-Distance-HOBO-v3.uf2) |
| Seeed Trail PIR + Rock + HOBO | [USB UF2](downloads/Trail-PIR-Rock-HOBO-v3.uf2) | [BLE OTA ZIP](downloads/Trail-PIR-Rock-HOBO-v3-OTA.zip) | [Matched Scout UF2](https://github.com/canyoncountryadventure/firmware/blob/Remote-Drone-Flashing-v2/downloads/Drone-Trail-PIR-Rock-HOBO-v3.uf2) |

Source branches: `field-self-recovery-v3` (canonical RAK + Seeed), `RAK-HOBO-Safe-v3`, `RAK-Soil-Moisture-HOBO-v3`, `RAK-Water-Distance-HOBO-v3`, `Seeed-HOBO-Safe-v3`, `Seeed-Water-Distance-HOBO-v3`, `Trail-Sensors-v3` (PIR + Rock + HOBO target). The dedicated trail SEN0171 counter has no HOBO and remains V2.

**Field validation:** CI success establishes compilation/packaging, not on-hardware RF telemetry or an end-to-end drone flash. Bench-check `LOGGER`, `READ`, `DFU`, one real logger record boundary, the matched target/Scout pair, and a power cycle before unattended deployment. Polling every 30 seconds cannot retrieve every historical record on a HOBO logging more frequently than that; V3 uses a fresh live `NEWREAD64` measurement rather than archive download.

---

## Archived Meshtastic Field Firmware — Field-Recovery v2

This repository contains the custom Meshtastic firmware used for the environmental sensor network, remote HOBO stations, water-level stations, soil-moisture stations, trail sensors, Heltec gateway, and remote drone flashing system.

**Field-Recovery v2 is the current hardened firmware family.** The v1 builds are retained for rollback and comparison.

The complete engineering audit and implementation specification is here:

**[Field-Recovery v2 — Final Audit, Architecture, and Build Specification](docs/FIELD_RECOVERY_V2_AUDIT_AND_BUILD_SPEC.md)**

## What changed in v2

The v2 family keeps the sensor logic from each existing firmware branch and replaces the common field reliability layer underneath it.

The main protections are:

- fixed SX1262 image-calibration timing with a 50 ms settle before RX-register restoration;
- no invasive radio maintenance during active RX, active TX, queued TX, or pending IRQ state;
- polling for missed **RX_DONE and TX_DONE** interrupts;
- recoverable SX1262 RX/radio state instead of assert-and-die behavior;
- actual SX1262 power-rail cycling on supported hardware such as the RAK4631;
- an independent nRF52840 field-health watchdog channel in addition to the main-loop watchdog;
- hardware-WDT escalation when radio/BLE recovery is exhausted;
- HOBO connection timeout and `sd_ble_gap_connect_cancel()`;
- HOBO STATUS/NEWREAD failures now rebuild the BLE link instead of retrying forever;
- HOBO state machine is the sole owner of its BLE scanner lifecycle;
- flash cache is quiesced before intentional nRF52 resets;
- larger nRF52 main-loop, BLE-task, and callback-task stacks;
- accurate reset-reason reporting;
- a **12-hour preventive reboot during v2 burn-in** as an independent final fallback;
- pinned build/framework revisions for reproducible firmware.

The original Meshtastic 2.7.26 application baseline is retained rather than blindly moving the field fleet to an unvalidated major firmware revision.

---

## Field-build drone-flash requirement

**Project release rule:** every new or revised nRF52 field-node firmware configuration must ship with remote drone-flash capability as part of the same release work unless the hardware is technically incompatible.

A field build is not complete until:

1. its normal target branch produces the standard USB UF2 and BLE OTA ZIP;
2. the target application contains a direct-message `DFU` hook that safely enters its BLE OTA bootloader and persists the post-update callback marker;
3. `Remote-Drone-Flashing-v2` produces a named `Drone-<configuration>.uf2` embedding the exact target OTA payload;
4. CI successfully builds both the normal target and its matched Scout image; and
5. hardware-specific bootloader behavior is documented rather than assuming the RAK implementation applies unchanged.

Seeed and other supported nRF52 targets use their own target-side bootloader handoff where required; the drone Scout remains a separate RAK4631 unless a future hardware change is explicitly documented.

---

## Download Field-Recovery v2

### nRF52840 environmental firmware

| Firmware | Board / sensors | USB UF2 | BLE DFU ZIP | Source |
|---|---|---|---|---|
| **RAK HOBO Safe v2** | RAK4631 / RAK19007 + HOBO MX2001/MX2201/MX2203 | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-HOBO-Safe-v2.uf2) | [ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-HOBO-Safe-v2-OTA.zip) | [branch](https://github.com/canyoncountryadventure/firmware/tree/RAK-HOBO-Safe-v2) |
| **RAK Soil Moisture + HOBO v2** | RAK4631 / RAK19007 + SEN0308 + optional HOBO | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-Soil-Moisture-HOBO-v2.uf2) | [ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-Soil-Moisture-HOBO-v2-OTA.zip) | [branch](https://github.com/canyoncountryadventure/firmware/tree/RAK-Soil-Moisture-HOBO-v2) |
| **RAK Water Distance + HOBO v2** | RAK4631 + ultrasonic water distance + HOBO | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-Water-Distance-HOBO-v2.uf2) | [ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-Water-Distance-HOBO-v2-OTA.zip) | [branch](https://github.com/canyoncountryadventure/firmware/tree/RAK-Water-Distance-HOBO-v2) |
| **RAK Water Distance v2** | RAK4631 + ultrasonic water distance | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-Water-Distance-v2.uf2) | [ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/RAK-Water-Distance-v2-OTA.zip) | [branch](https://github.com/canyoncountryadventure/firmware/tree/RAK-Water-Distance-v2) |
| **Seeed HOBO Safe v2** | XIAO nRF52840 + Wio-SX1262 + HOBO | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Seeed-HOBO-Safe-v2.uf2) | [ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Seeed-HOBO-Safe-v2-OTA.zip) | [branch](https://github.com/canyoncountryadventure/firmware/tree/Seeed-HOBO-Safe-v2) |
| **Seeed Water Distance + HOBO v2** | XIAO nRF52840 + Wio-SX1262 + water distance + HOBO | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Seeed-Water-Distance-HOBO-v2.uf2) | [ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Seeed-Water-Distance-HOBO-v2-OTA.zip) | [branch](https://github.com/canyoncountryadventure/firmware/tree/Seeed-Water-Distance-HOBO-v2) |
| **Seeed Water Distance v2** | XIAO nRF52840 + Wio-SX1262 + ultrasonic water distance | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Seeed-Water-Distance-v2.uf2) | [ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Seeed-Water-Distance-v2-OTA.zip) | [branch](https://github.com/canyoncountryadventure/firmware/tree/Seeed-Water-Distance-v2) |
| **Trail PIR + Rock + HOBO v2** | Seeed trail/presence/rock telemetry + HOBO | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Trail-PIR-Rock-HOBO-v2.uf2) | [ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Trail-PIR-Rock-HOBO-v2-OTA.zip) | [branch](https://github.com/canyoncountryadventure/firmware/tree/Trail-Sensors-v2) |
| **Trail SEN0171 v2** | Dedicated fast trail counter | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Trail-SEN0171-v2.uf2) | [ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Trail-SEN0171-v2-OTA.zip) | [branch](https://github.com/canyoncountryadventure/firmware/tree/Trail-Sensors-v2) |

### Canonical recovery builds

These are useful for testing the common v2 recovery layer without a product-specific sensor branch.

| Build | UF2 | BLE DFU ZIP | Source |
|---|---|---|---|
| **Field Self-Recovery v2 — RAK4631** | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Field-Self-Recovery-v2-RAK4631.uf2) | [ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Field-Self-Recovery-v2-RAK4631-OTA.zip) | [field-self-recovery-v2](https://github.com/canyoncountryadventure/firmware/tree/field-self-recovery-v2) |
| **Field Self-Recovery v2 — Seeed** | [UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Field-Self-Recovery-v2-Seeed.uf2) | [ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Field-Self-Recovery-v2-Seeed-OTA.zip) | [field-self-recovery-v2](https://github.com/canyoncountryadventure/firmware/tree/field-self-recovery-v2) |

### Remote drone flashing

Every RAK v2 target branch keeps its **normal UF2 + normal OTA ZIP** and contains the mesh `DFU` hook. The **compressed Scout images are centralized on [Remote-Drone-Flashing-v2](https://github.com/canyoncountryadventure/firmware/tree/Remote-Drone-Flashing-v2)**.

| Target configuration | Compressed drone/Scout UF2 |
|---|---|
| RAK HOBO Safe v2 | [Drone-RAK-HOBO-Safe-v2.uf2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Remote-Drone-Flashing-v2/downloads/Drone-RAK-HOBO-Safe-v2.uf2) |
| RAK Soil Moisture + HOBO v2 | [Drone-RAK-Soil-Moisture-HOBO-v2.uf2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Remote-Drone-Flashing-v2/downloads/Drone-RAK-Soil-Moisture-HOBO-v2.uf2) |
| RAK Water Distance v2 | [Drone-RAK-Water-Distance-v2.uf2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Remote-Drone-Flashing-v2/downloads/Drone-RAK-Water-Distance-v2.uf2) |
| RAK Water Distance + HOBO v2 | [Drone-RAK-Water-Distance-HOBO-v2.uf2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Remote-Drone-Flashing-v2/downloads/Drone-RAK-Water-Distance-HOBO-v2.uf2) |
| Canonical Field Self-Recovery v2 — RAK4631 | [Drone-Field-Self-Recovery-v2-RAK4631.uf2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Remote-Drone-Flashing-v2/downloads/Drone-Field-Self-Recovery-v2-RAK4631.uf2) |
| Seeed Water Distance + HOBO v2 | [Drone-Seeed-Water-Distance-HOBO-v2.uf2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Remote-Drone-Flashing-v2/downloads/Drone-Seeed-Water-Distance-HOBO-v2.uf2) |

Each Scout UF2 embeds an **LZ4-compressed copy of the matching target branch's application OTA**. Flash the normal target firmware onto the field node; flash the matching `Drone-*.uf2` onto the separate RAK4631 carried by the drone.

The original physically proven `2.7.26.b812974` pair remains the hardware-validation reference. The new multi-configuration v2 images are CI-built successfully but should receive one repeat physical end-to-end bench DFU before being trusted for inaccessible field nodes.

### Heltec gateway

| Firmware | Board | Package | Source |
|---|---|---|---|
| **Heltec Gateway v2** | Heltec WiFi LoRa 32 V4 | [Full build ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/field-self-recovery/downloads/Heltec-Gateway-v2.zip) | [branch](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v2) |

The Heltec gateway retains the existing mesh -> Vercel -> Neon -> dashboard pipeline and gateway-specific sensor handling. Its v2 branch also carries the shared SX1262 calibration, missed-IRQ, and radio-state recovery core. The nRF52840-only dual-WDT/flash-reset logic does not apply to the ESP32-S3 gateway.

---

## Which file do I flash?

| File | Use |
|---|---|
| **`.uf2`** | USB installation/recovery on RAK4631 or Seeed XIAO nRF52840. Double-press reset and copy the UF2 to the bootloader drive. |
| **`-OTA.zip`** | BLE update through Nordic nRF Connect: **DFU -> Distribution packet (ZIP)**. Do not unzip it. |
| **Heltec full ZIP** | ESP32-S3/Heltec V4 gateway bundle. |
| **Drone Scout UF2** | Firmware for the separate RAK4631 that performs autonomous BLE flashing of the remote target. |

Do not select a UF2 as a Nordic DFU package, and do not copy the OTA ZIP to the UF2 bootloader drive.

---

## v2 recovery architecture

```text
                     FIELD-RECOVERY v2

Normal LoRa operation
        |
        +-- missed RX/TX IRQ poll
        |
        +-- guarded 60 s SX1262 maintenance
        |       |
        |       +-- 50 ms CalibrateImage settle
        |       +-- restore RX state / 0x8B5
        |
        +-- radio operation fails
                |
                +-- RX retry
                +-- full SX1262 re-init
                +-- SX1262 rail power-cycle
                +-- retry RX
                |
                +-- still unhealthy
                        |
                        +-- field WDT channel stops feeding
                        +-- nRF52840 hardware reset

HOBO builds add:
scan -> connect -> STATUS/NEWREAD -> reconnect/cancel -> WDT escalation

Independent burn-in fallback:
12-hour flash-safe whole-node reboot
```

On a RAK4631 reboot, v2 also explicitly cold-cycles the SX1262 power-enable rail before normal radio initialization.

---

## Sensor functionality retained

The v2 branches were **not** created by flattening everything into one generic build. Each v2 branch starts with its matching sensor branch and receives the same hardened core underneath it.

| Product | Existing function retained in v2 |
|---|---|
| HOBO Safe | MX2001 / MX2201 / MX2203 STATUS pointer tracking, NEWREAD, LOCK/UNLOCK, manual READ |
| Soil Moisture + HOBO | SEN0308 ADC reading/calibration + HOBO |
| Water Distance | existing UART distance drivers, stage calibration, verification, interval persistence |
| Water Distance + HOBO | both water and HOBO subsystems |
| Trail PIR/Rock/HOBO | presence/PIR, rock telemetry, HOBO |
| SEN0171 | dedicated trail-count detection |
| Remote Drone | embedded OTA target + autonomous BLE target updater |
| Heltec Gateway | Wi-Fi gateway, mesh ingest, Vercel/Neon forwarding and gateway sensor support |

---

## HOBO v2 DM commands

Commands are case-insensitive. A leading `/` is optional where supported.

| Command | Function |
|---|---|
| `PING` | Radio/application liveness check |
| `STATUS` / `HEALTH` | Uptime, power, BLE link count, TX/recovery information, reset reason |
| `LOGGER` | HOBO identity/model/MAC/interval/lock information |
| `READ` | Fresh HOBO read without consuming the automatic pointer |
| `LOCK` / `UNLOCK` | Persist or clear logger assignment |
| `POWER` | Battery and charging status |
| `BLE` | BLE central-link/scanner state |
| `AUTO` | Automatic pointer-gated telemetry status |
| `WATCHDOG` | Main + field watchdog information |
| `STATS` | TX and radio-recovery counters |
| `NODES` | Mesh node count |
| `UPTIME` | Uptime |
| `VERSION` | v2/platform/recovery identity |
| `RECOVER` / `REBOOT` | Flash-safe whole-node reboot |

In v2, stale scanner/link recovery is automatic. The HOBO state machine owns scanner lifecycle rather than allowing a second recovery thread to manipulate it concurrently.

---

## Water distance commands

The existing water command surface remains available. Combined HOBO builds use the explicit `WATER` prefix when needed.

Common commands include:

```text
STATUS
RAW
READ
VERIFY
SENSOR A01NYUB
SENSOR A02YYUW
SENSOR SEN0590
SENSOR AUTO
INTERVAL 1H
CAL STAGE 1.42FT
CAL STATUS
CAL UNLOCK CONFIRM
CAL RESET CONFIRM
TELEMETRY NOW
RESET WATER CONFIRM
```

For a combined build:

```text
WATER STATUS
WATER READ
WATER VERIFY
WATER INTERVAL 1H
WATER CAL STAGE 1.42FT
WATER TELEMETRY NOW
```

---

## RAK soil moisture wiring

For the SEN0308 on the RAK19007:

| SEN0308 | RAK19007 |
|---|---|
| Red | `VDD` regulated 3.3 V |
| Yellow | `AIN1` |
| Black | `GND` |

The soil firmware retains its hourly automatic telemetry and raw ADC value for later recalibration.

---

## Build/source branches

| Branch | Role |
|---|---|
| [field-self-recovery-v2](https://github.com/canyoncountryadventure/firmware/tree/field-self-recovery-v2) | canonical common RAK/Seeed v2 recovery baseline |
| [RAK-HOBO-Safe-v2](https://github.com/canyoncountryadventure/firmware/tree/RAK-HOBO-Safe-v2) | RAK HOBO |
| [RAK-Soil-Moisture-HOBO-v2](https://github.com/canyoncountryadventure/firmware/tree/RAK-Soil-Moisture-HOBO-v2) | RAK soil + HOBO |
| [RAK-Water-Distance-HOBO-v2](https://github.com/canyoncountryadventure/firmware/tree/RAK-Water-Distance-HOBO-v2) | RAK water + HOBO |
| [RAK-Water-Distance-v2](https://github.com/canyoncountryadventure/firmware/tree/RAK-Water-Distance-v2) | RAK water |
| [Seeed-HOBO-Safe-v2](https://github.com/canyoncountryadventure/firmware/tree/Seeed-HOBO-Safe-v2) | Seeed HOBO |
| [Seeed-Water-Distance-HOBO-v2](https://github.com/canyoncountryadventure/firmware/tree/Seeed-Water-Distance-HOBO-v2) | Seeed water + HOBO |
| [Seeed-Water-Distance-v2](https://github.com/canyoncountryadventure/firmware/tree/Seeed-Water-Distance-v2) | Seeed water |
| [Trail-Sensors-v2](https://github.com/canyoncountryadventure/firmware/tree/Trail-Sensors-v2) | PIR/Rock/HOBO + SEN0171 |
| [Remote-Drone-Flashing-v2](https://github.com/canyoncountryadventure/firmware/tree/Remote-Drone-Flashing-v2) | drone/Scout + matched hardened target |
| [Heltec-Gateway-v2](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v2) | Heltec V4 gateway |

---

## Reproducibility

The nRF52 v2 builds pin the Meshtastic Adafruit nRF52 framework revision to:

```text
0fd295f13203e93df19d578073646ec32f2bf45a
```

The product workflows that invoke the Meshtastic firmware build action directly pin:

```text
39d0ffe8e0708beb3fb7b66f4c91aa941dc9764e
```

This is intentional. A firmware filename should not silently change because an external `main` branch moved.

---

## Legacy / rollback firmware

The v1 builds remain in the repository and are not being deleted. The previous front-page distribution directory remains available at:

**[Self-Recovery-v1s](https://github.com/canyoncountryadventure/firmware/tree/field-self-recovery/Self-Recovery-v1s)**

The proven v1 remote-drone pair also remains available on:

**[Remote-Drone-Flashing](https://github.com/canyoncountryadventure/firmware/tree/Remote-Drone-Flashing)**

Use legacy firmware when intentionally rolling back or when comparing v1/v2 behavior during field testing.

---

## Known limits

Firmware cannot recover a station from:
- actual loss of electrical power;
- failed MCU or failed SX1262;
- damaged antenna/feedline;
- destroyed/corrupt bootloader;
- physical/environmental damage severe enough to prevent boot.

Field-Recovery v2 is intended to prevent a **powered, electrically functional** station from remaining indefinitely stranded because of recoverable LoRa state loss, missed IRQs, stale BLE state, stuck HOBO connections, or ordinary firmware runtime failure.
