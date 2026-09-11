# Meshtastic Field Self-Recovery Firmware

> **Canonical development baseline:** `field-self-recovery`
>
> **Purpose:** unattended nRF52 field nodes that must survive solar/battery cycling, BLE failures, sensor failures, and long remote deployments without destructive resets.

This repository contains custom Meshtastic firmware for environmental monitoring, trail counters, and remote sensor nodes. The `field-self-recovery` branch is the common starting point for new field firmware.

## Canonical hardware targets

- **Seeed XIAO nRF52840 + Wio-SX1262**
- **RAK4631 on RAK19007** for new WisBlock field deployments

Existing Meshtastic RAK4631 board support remains compatible with other supported WisBlock bases, but new distance-sensor work in this project is standardized on RAK19007.

## Recovery / solar baseline

The canonical field baseline carries the protections validated in the September 2026 recovery builds:

- nRF52840 hardware watchdog
- non-destructive remote `RECOVER` / `REBOOT`
- low-duty BLE disconnected-state recovery
- automatic long-disconnect BLE-stack reboot
- persistent field configuration
- battery/device telemetry support
- safer low-voltage margin for solar nodes
- Seeed LPCOMP battery-rise wake from SYSTEM OFF after solar recharge
- RAK LPCOMP wake retained with safer low-voltage handling
- recovery actions preserve NVS, node identity, NodeDB, channels, keys, and saved sensor configuration

The recovery supervisor is intentionally separated from sensor protocol code. A new sensor project should inherit this baseline instead of reimplementing recovery.

See [`FIELD_SELF_RECOVERY.md`](FIELD_SELF_RECOVERY.md) for the canonical baseline details.

## Current validated HOBO support

The recovery baseline was developed around the existing universal HOBO reader and supports:

- Onset MX2001
- Onset MX2201
- Onset MX2203

Automatic HOBO telemetry follows the logger write pointer and `NEWREAD64` rather than pretending the radio interval is the logger interval. Manual `READ` does not consume the automatic pointer.

### Existing production source lines

- `hobo-self-recovery-seeed` — validated Seeed recovery + solar-wake build
- `hobo-self-recovery-rak4631` — validated RAK4631 recovery + solar-safe build
- `heltec-gateway-clean-rebuild` — Heltec V4 internet/cloud gateway

These remain useful immutable reference/deployment lines. New field development should branch from `field-self-recovery`.

## Distance-sensor development

New distance work lives under:

```text
src/modules/Telemetry/DistanceSensor/
```

Development branches:

- `distance-self-recovery-seeed`
- `distance-self-recovery-rak4631`

Initial sensor support is planned for:

- DFRobot **SEN0590** — I2C
- DFRobot **SEN0311 / A02YYUW** — UART
- DFRobot **SEN0313 / A01NYUB** — UART

The architecture separates sensor drivers from the application mode:

```text
DistanceSensorModule
├── persistent configuration / calibration
├── MODE WATER
│   ├── raw distance
│   ├── CAL STAGE <known stage>
│   └── derived stage
├── MODE TRAIL
│   ├── CAL CLEAR
│   ├── persistent clear-path baseline
│   ├── trigger / clear hysteresis
│   ├── fast rearm
│   └── daily + lifetime counts
└── drivers
    ├── SEN0590
    ├── SEN0311_A02YYUW
    └── SEN0313_A01NYUB
```

The same water/trail logic should work on either board and with any supported distance driver.

## Field configuration principles

Water deployment should be configurable after physical installation. Example:

```text
MODE WATER
CAL STAGE 1.42FT
READ
STATUS
```

The node records the current raw sensor distance and derives the installed reference automatically. A later reading becomes:

```text
stage = saved_reference - raw_distance
```

Raw distance is retained so derived stage remains auditable.

Trail deployment should likewise be calibrated in place:

```text
MODE TRAIL
CAL CLEAR
TRIGGER 12IN
COUNT
STATUS
```

`CAL CLEAR` should learn the actual clear-path sensor baseline rather than requiring a hard-coded trail width. Detection uses hysteresis and requires the beam to clear before another person is counted.

## Data integrity rules

- Preserve raw measurements.
- Flag questionable records rather than silently replacing them.
- Do not auto-recalibrate around obstructions without an explicit command.
- Persist calibration and counters across reboot and battery loss.
- Do not transmit high-rate raw trail samples over LoRa; transmit compact event/summary data.
- Water telemetry should include raw distance, derived stage, sensor health, battery state, and timestamp.

## Recovery safety rules

Normal updates and recovery must **not** erase flash or NVS.

Do not use destructive factory images or erase operations for ordinary firmware updates. Preserve:

- Meshtastic node identity
- channels and PSKs
- NodeDB
- Wi-Fi / radio settings where applicable
- sensor calibration
- logger locks
- persistent counters

## CI

`field-self-recovery` builds both canonical nRF52 targets on every push:

- `seeed_xiao_nrf52840_kit`
- `rak4631`

A change to the common baseline is not considered ready until both board builds pass.
