# Fucking Around — Experimental Meshtastic Water + HOBO Field Node

> **Branch:** `fucking-around`
>
> **Status:** Experimental/test firmware only. **Do not use as the canonical HOBO production branch.**
>
> **Hardware:** Seeed XIAO nRF52840 + Wio-SX1262
>
> **PlatformIO target:** `seeed_xiao_nrf52840_kit`
>
> **Paired experimental gateway:** `fucking-around-heltec`

This branch is for testing ultrasonic water-level behavior over Meshtastic while preserving the universal HOBO MX2001 / MX2201 / MX2203 reader and command set.

## Current water test

Sensor: **DFRobot A02YYUW / SEN0311 waterproof ultrasonic**

Wiring:

| A02YYUW | Seeed XIAO |
|---|---|
| VCC | 3V3 |
| GND | GND |
| TX | D7 / UART RX |
| RX/control | floating |

Current cat-water calibration:

```text
100% full = 224 mm / 8.82 in sensor-to-water
0% full   = 406.4 mm / 16.00 in sensor-to-bottom
```

## Sampling and alert filtering

The water path uses two filtering layers.

### Minute-level sample

Each minute the node collects up to 9 valid UART distance readings and uses the **median** as that minute's sample.

### Persistent alert confirmation

The node keeps the latest 5 valid minute-level samples. Once the window is full it:

1. sorts the 5 samples;
2. discards the highest;
3. discards the lowest;
4. averages the middle 3;
5. uses that filtered value for threshold and refill decisions.

A single bad minute sample therefore cannot trigger a threshold alert by itself.

Thresholds are:

```text
90, 80, 70, 60, 50, 40, 30, 20, 10, 0%
```

Sensor-health alerts remain separate: fault after 3 consecutive failed minute samples and recovery on the first subsequent valid sample.

## Water direct-message commands

Commands are case-insensitive and may optionally begin with `/`.

| Command | Function |
|---|---|
| `WATER` / `WATER STATUS` | Latest percent, distance, health, filtered alert state, and sample age |
| `READ WATER` / `WATER NOW` | Force a fresh ultrasonic sample |
| `WATER RAW` | Latest median distance, valid-frame count, failure count, sample age, and rolling-filter information |
| `WATER HELP` | Water command reference |

## HOBO support remains present

The universal HOBO command set is unchanged:

```text
LOGGER
READ
LOCK
UNLOCK
```

The HOBO production behavior in this branch still comes from the universal MX2001/MX2201/MX2203 implementation. Automatic HOBO transmission remains tied to confirmed logger record/write-pointer changes rather than a blind timer.

## Experimental alert path

```text
A02YYUW ultrasonic
    ↓ UART
Seeed XIAO + Wio-SX1262 (`fucking-around`)
    ↓ Meshtastic WATER_ALERT|...
mesh / relays
    ↓
Heltec V4 OLED (`fucking-around-heltec`)
    ↓ Wi-Fi
GitHub issue
```

The paired Heltec branch records the alert and available source/RF metadata in GitHub. This experimental water-alert path does not replace the canonical multi-station cloud gateway.

## Production separation

Canonical production branches are:

| Role | Branch |
|---|---|
| Universal HOBO field node | `hobo-mx2001-mx2201-mx2203` |
| Heltec sensor/cloud gateway | `cca-heltec-sensor-gateway` |

Do not merge experimental water behavior into those branches until the complete sensor → mesh → gateway path has been intentionally field-tested and accepted.

## Build

```powershell
py -m platformio run -e seeed_xiao_nrf52840_kit
```

After a successful build, flash the application UF2 for `seeed_xiao_nrf52840_kit`. Do not use factory-erase images unless intentionally wiping the device.

## Detailed notes

See:

```text
docs/fucking-around-water.md
```

## Branch rules

1. This is an experimental branch.
2. Keep the paired gateway work on `fucking-around-heltec`.
3. Preserve the universal HOBO `LOGGER`, `READ`, `LOCK`, `UNLOCK`, and automatic pointer-gated telemetry behavior while testing water features.
4. Keep ultrasonic calibration/test assumptions out of production until field validated.
5. Do not use factory erase as a normal firmware-update step.
6. Promote only tested behavior into the canonical production branches.
