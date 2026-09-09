# Fucking Around — Meshtastic Water + HOBO Test Firmware

Experimental Seeed XIAO nRF52840 + Wio-SX1262 firmware for testing water-level sensors over Meshtastic while preserving the existing HOBO MX2001 / MX2201 / MX2203 support.

> Branch: `fucking-around`  
> Hardware target: `seeed_xiao_nrf52840_kit`  
> Current build: `2.7.26.c8b1d7a`

## Download current Seeed firmware

**GitHub Actions build:** [Build Fucking Around Seeed — run 34386111305](https://github.com/canyoncountryadventure/firmware/actions/runs/34386111305)

**Build artifact:** [firmware-nrf52840-seeed_xiao_nrf52840_kit-fucking-around](https://github.com/canyoncountryadventure/firmware/actions/runs/34386111305/artifacts/10117892444)

After downloading and extracting the artifact, flash:

`firmware-seeed_xiao_nrf52840_kit-2.7.26.c8b1d7a.uf2`

Do **not** use either factory-erase UF2 unless intentionally wiping the device.

## Current water test

Sensor: **DFRobot A02YYUW / SEN0311 waterproof ultrasonic**

Wiring:

- A02YYUW VCC -> XIAO 3V3
- A02YYUW GND -> XIAO GND
- A02YYUW TX -> XIAO D7 / UART RX
- A02YYUW RX/control -> floating

Calibration for the current cat-water test:

- 100% full = 224 mm / 8.82 in sensor-to-water
- 0% full = 406.4 mm / 16.00 in sensor-to-bottom

## Outlier-resistant water alerts

The alert system now has two filtering layers.

### Layer 1 — each minute

The node collects up to 9 valid ultrasonic UART readings and uses their **median** as that minute's distance sample.

### Layer 2 — alert confirmation

The node keeps the latest **5 valid minute-level samples**. Before an alert can fire it:

1. sorts the 5 samples;
2. removes the highest sample;
3. removes the lowest sample;
4. averages the middle 3;
5. uses that filtered average for threshold and refill decisions.

A single bad minute-level reading therefore cannot trigger a water-level threshold warning. A real change must persist across multiple samples.

Threshold alerts remain at:

`90, 80, 70, 60, 50, 40, 30, 20, 10, 0%`

Sensor-fault behavior remains separate: a fault alert is sent after 3 consecutive failed minute samples, with a recovery alert after valid readings return.

## Meshtastic water commands

Direct-message the Seeed node. Commands are case-insensitive and may optionally start with `/`.

- `WATER` / `WATER STATUS` — latest water status, including the filtered alert value/window state
- `READ WATER` / `WATER NOW` — force a fresh ultrasonic sample
- `WATER RAW` — raw/median sample details plus alert-filter information
- `WATER HELP` — command list

The existing HOBO commands remain available and unchanged:

- `LOGGER`
- `READ`
- `LOCK`
- `UNLOCK`

## Alert path

Current tested path:

```text
A02YYUW ultrasonic
    ↓ UART
Seeed XIAO + Wio-SX1262
    ↓ Meshtastic text: WATER_ALERT|...
mesh / relays
    ↓
Home Heltec V4 OLED
    ↓ Wi-Fi
GitHub issue
```

The Heltec gateway recognizes `WATER_ALERT|...` messages and creates a GitHub issue with source node, alert data, RSSI, SNR, hops, and packet ID. The water-alert path does not write to Neon.

## Detailed water-test notes

See [`docs/fucking-around-water.md`](docs/fucking-around-water.md).

## Production firmware remains separate

This branch is experimental. Production HOBO firmware remains on:

`hobo-mx2001-mx2201-mx2203`

The experimental water work must not be merged into production until field testing is complete.
