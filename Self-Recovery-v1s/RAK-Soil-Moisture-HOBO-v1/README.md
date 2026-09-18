# RAK Soil Moisture + HOBO v1

Production firmware for **RAK4631 + RAK19007**, a **DFRobot SEN0308** analog soil-moisture sensor, and optional **HOBO MX2001/MX2201/MX2203** BLE logger.

## Downloads

- **[USB UF2](../../downloads/RAK-Soil-Moisture-HOBO-v1.uf2)**
- **[BLE DFU ZIP](../../downloads/RAK-Soil-Moisture-HOBO-v1-OTA.zip)**
- **[Complete wiring, calibration, commands, packet format, validation and troubleshooting](https://github.com/canyoncountryadventure/firmware/tree/RAK-Soil-Moisture-HOBO-v1)**

## SEN0308 wiring

| Lead | RAK19007 |
|---|---|
| Red | `VDD` — regulated 3.3 V, not `VBAT` |
| Yellow | `AIN1` |
| Black | `GND` |
| Black | `GND` |

`AIN1` maps to RAK4631 A1 / P0.31 / AIN7.

## First commands

```text
PING
WATCHDOG
POWER
SOIL READ
SOIL STATUS
SOIL TX
LOGGER
LOCK
AUTO
BLE
STATUS
```

Automatic soil telemetry is fixed at one hour. Current field endpoints are ADC10 580 = 0% and ADC10 0 = 100%. The normal telemetry packet carries `soil_moisture`; a second compact private packet preserves the raw ADC10 value.
