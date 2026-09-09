# Fucking Around — A02YYUW Cat Bowl Water Monitor

Experimental branch: `fucking-around`

## Current build

- Version: `2.7.26.c8b1d7a`
- Target: `seeed_xiao_nrf52840_kit`
- GitHub Actions run: https://github.com/canyoncountryadventure/firmware/actions/runs/34386111305
- Artifact: https://github.com/canyoncountryadventure/firmware/actions/runs/34386111305/artifacts/10117892444
- Flash file inside artifact: `firmware-seeed_xiao_nrf52840_kit-2.7.26.c8b1d7a.uf2`

## Hardware

- Seeed XIAO nRF52840 + Wio-SX1262
- DFRobot A02YYUW / SEN0311
- Sensor TX -> XIAO D7 / RX
- Sensor VCC -> 3V3
- Sensor GND -> GND
- Sensor RX / blue control wire -> floating

## Calibration

- 100% full: 224 mm / 8.82 in sensor-to-water
- 0% full: 406.4 mm / 16.00 in sensor-to-bottom
- Percent full is linear between those two calibration points.

## Sampling and alert filtering

The node samples every 60 seconds.

### Minute-level sample

Each minute it:

- collects up to 9 valid A02YYUW UART readings;
- requires at least 5 valid frames;
- sorts the valid readings;
- uses the median distance as that minute's sample.

### Persistent alert confirmation

Threshold/refill alerts do not use a single minute sample directly.

The node keeps the latest 5 valid minute-level samples. Once the 5-sample window is full it:

1. sorts the five minute samples;
2. discards the highest;
3. discards the lowest;
4. averages the middle three;
5. converts that filtered distance to percent full;
6. uses only that filtered value for water-level threshold/refill alerts.

This prevents one isolated bad minute sample from generating a false water alert. A change must persist across multiple samples before the filtered value crosses a threshold.

Sensor-health alerts remain independent:

- sensor fault after 3 consecutive failed minute samples;
- sensor recovered alert on the first subsequent valid sample.

## Meshtastic commands

Direct-message the node with any of the following. Commands are case-insensitive and may optionally start with `/`.

- `WATER` — latest percent/distance, health, next alert threshold, sample age, and filtered alert state
- `WATER STATUS` — same as `WATER`
- `READ WATER` — force a fresh 9-reading median sample and return it
- `WATER NOW` — alias for `READ WATER`
- `WATER RAW` — latest median distance, valid-frame count, failure count, sample age, and rolling-filter information
- `WATER HELP` — command list

The existing HOBO commands remain available and unchanged, including `READ`, `LOGGER`, `LOCK`, and `UNLOCK`.

## Alert behavior

The node broadcasts a short `WATER_ALERT|...` Meshtastic text message when the **filtered 5-sample result** crosses:

- 90%
- 80%
- 70%
- 60%
- 50%
- 40%
- 30%
- 20%
- 10%
- 0%

A refill alert is generated when the filtered level rises by at least 10 percentage points. A refill resets the downward threshold ladder.

## Tested alert path

```text
A02YYUW
  ↓ UART
Seeed XIAO + Wio-SX1262
  ↓ Meshtastic WATER_ALERT|...
mesh / relays
  ↓
Home Heltec V4 OLED
  ↓ Wi-Fi
GitHub issue
```

The Heltec records source node, raw alert, LoRa RSSI/SNR, hop count, and packet ID in the GitHub issue. The water-alert path does not write to Neon.

## Data storage

This experimental water path intentionally has no Neon/database upload. Readings stay on the node except for Meshtastic command replies and alert messages.
