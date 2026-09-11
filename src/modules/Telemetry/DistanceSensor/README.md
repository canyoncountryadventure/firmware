# Distance Sensor Module

Shared home for field-configurable distance sensing on Seeed XIAO nRF52840 + Wio-SX1262 and RAK4631 + RAK19007.

## Planned sensors

- DFRobot SEN0590 — I2C
- DFRobot SEN0311 / A02YYUW — UART
- DFRobot SEN0313 / A01NYUB — UART

## Architecture

```text
DistanceSensorModule
├── shared configuration + persistent calibration
├── MODE WATER
│   ├── raw sensor distance
│   ├── CAL STAGE <value>
│   └── derived stage
├── MODE TRAIL
│   ├── CAL CLEAR
│   ├── persistent baseline
│   ├── trigger + clear hysteresis
│   ├── fast rearm
│   └── daily + lifetime counts
└── drivers
    ├── SEN0590
    ├── SEN0311_A02YYUW
    └── SEN0313_A01NYUB
```

The sensor driver is intentionally separated from water/trail behavior. Changing the installed sensor must not require rewriting calibration, counting, recovery, battery, or Meshtastic logic.

## Field configuration goals

Water installation example:

```text
MODE WATER
CAL STAGE 1.42FT
READ
STATUS
```

Trail installation example:

```text
MODE TRAIL
CAL CLEAR
TRIGGER 12IN
COUNT
STATUS
```

Calibration and counters must survive reboot, watchdog recovery, and battery loss. Firmware recovery must never erase Meshtastic NVS, channels, keys, node identity, or saved distance configuration.

## Telemetry principles

- Preserve raw distance.
- Derive stage from saved calibration rather than replacing the raw measurement.
- Do not transmit every high-rate trail sample over LoRa.
- Trail events should increment local persistent counters and transmit compact event/summary telemetry.
- Water measurements should include raw distance, derived stage, sensor health, battery state, and timestamp.
