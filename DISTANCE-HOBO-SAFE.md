# Distance + logger safe 1.0

Combined RAK4631 (RAK19003 or RAK19007 base) and Seeed XIAO nRF52840 kit firmware.
Distance drivers retained: SEN0590 I2C, SEN0311/A02YYUW UART, SEN0313/A01NYUB UART, generic compatible UART.
BLE logger models retained: MX2001, MX2201, MX2203, using the existing universal NEWREAD64 reader.
Existing environmental drivers are retained. GPS and generic serial forwarding are disabled because the ultrasonic reserves Serial1.

## Flash

- UF2: USB bootloader drive; copy the file for your board. Do not factory erase.
- ZIP: Nordic legacy DFU application package for nRF Connect. Select ZIP without extracting it. It is not a UF2 bundle.
- RAK file is for RAK4631 nRF52840, not ESP32 RAK or RAK4631-R AT firmware.
- Existing identity, keys, channels, logger lock and distance settings are not intentionally erased by this application update.
- Build/package checks are not a physical BLE DFU or field hardware test. USB remains the preferred recovery path given earlier 100% stalls.

## Field DM commands

Send directly to the sensor radio. Commands are case-insensitive; a leading slash is accepted.
Shared bare READ/STATUS/HEALTH remain logger/recovery commands; use DIST to address distance.

| Command | Purpose |
| --- | --- |
| HELP | Logger/recovery and distance help replies |
| DIST HELP | Distance-only help |
| READ | Manual BLE logger read; does not consume the automatic read pointer |
| LOGGER | Logger link, model, MAC and logging interval |
| LOCK / UNLOCK | Existing logger selection lock commands |
| AUTO | Automatic logger reading/supervisor status, not an on/off setter |
| STATUS / HEALTH | Recovery health |
| BLE | Passive 10% scanner diagnostics (not total system duty) |
| POWER / BATTERY | Battery diagnostics; unavailable ADC is not a real battery reading |
| WATCHDOG | Actual watchdog register timeout and run-in-sleep mode |
| STATS / NODES / UPTIME / VERSION | Recovery counters, node count, uptime and combined build identification |
| PING / WAKE | Connectivity check |
| SCAN / RECONNECT | Existing conservative scanner recovery commands |
| RECOVER / REBOOT | Safe full-system restart; requires a responsive node |
| DIST STATUS / DIST HEALTH | Distance mode, calibration, driver, interval and read errors |
| DIST SENSOR | Configured and active driver |
| DIST SENSOR AUTO | Detect compatible driver |
| DIST SENSOR SEN0311 / DIST SENSOR A02YYUW | Select 3–450 cm UART sensor |
| DIST SENSOR SEN0313 / DIST SENSOR A01NYUB | Select 28–750 cm UART sensor |
| DIST SENSOR SEN0590 | Select I2C driver |
| DIST SENSOR UART | Select compatible generic four-byte UART protocol |
| DIST MODE WATER | Measure/report at the configured interval |
| DIST MODE TRAIL | Local repeated sampling, periodic count summary |
| DIST MODE IDLE | Stop automatic distance sampling; logger continues |
| DIST READ / DIST RAW | Current measurement / raw distance |
| DIST CAL STAGE 1.42FT | Calibrate against known water stage with a fresh median measurement |
| DIST CAL CLEAR | Calibrate unobstructed trail baseline |
| DIST CAL RESET | Clear calibration, not identity or keys |
| DIST TRIGGER 12IN | Trail trigger distance closer than clear baseline |
| DIST CLEAR 6IN | Trail re-arm hysteresis; must be below trigger |
| DIST INTERVAL 1H | Distance reporting interval; units S/M/H, 5 sec–24 hr |
| DIST COUNT | Daily/lifetime trail counters |
| DIST RESET DAILY / DIST RESET COUNT | Reset daily only / daily and lifetime counts |
| DIST TELEMETRY NOW | Queue a distance packet now |

Bare unambiguous distance commands (MODE, SENSOR, CAL, RAW, COUNT, INTERVAL, etc.) also work.
Distance parameters must include units MM, CM, M, IN or FT. Wait at least 8 seconds after boot before configuring distance.

### A02YYUW water setup example

```
VERSION
LOGGER
DIST SENSOR A02YYUW
DIST MODE WATER
DIST INTERVAL 1H
DIST RAW
DIST CAL STAGE 1.42FT
DIST READ
DIST STATUS
```

Use your actual measured stage instead of 1.42FT. Stage = reference minus raw distance; raw is never overwritten.
Settings and calibration persist in /prefs/distance_sensor.bin. Moving or changing sensors requires recalibration.
Distance remains the existing DS v1 binary packet on PRIVATE_APP; logger remains environmental telemetry.
This build does not add a dashboard distance decoder or imply that standard Meshtastic displays private distance packets.

## Wiring and power

- Seeed default kit: sensor TX to D7 (MCU RX), common GND, compatible supply. D6 is MCU TX and is not needed for the streaming sensor.
- RAK: sensor TX to UART1 RX (Arduino logical pin 15), common GND, compatible supply. Logical pin 15 is not a physical connector pin number; verify the RAK19003 pad/header location before soldering.
- SEN0590 uses Wire; Seeed default kit routes I2C to NFC pins 30/31, not radio SPI pins D4/D5.
- Never drive nRF52840 inputs above 3.3 V. Power/wiring depends on the sensor model.
- The sensor is not power-gated by this firmware; IDLE stops reads but cannot turn off a directly wired sensor.
- One logger recovery supervisor tunes passive scans to 10% and refreshes disconnected scans. Core watchdog ownership is preserved.
- Solar wake comparator and 3.40 V empty threshold from the safe base remain. They rely on real battery sensing and wiring; external charger power loss cannot be fixed in software.
- A six-hour disconnected scanner recovery reboot does not guarantee recovery from every BLE failure or electrical problem.
