# CCA MX + HOBO + PIR + Rock — Seeed XIAO

**Branch:** `CCA-MX-HOBO-PIR-ROCK-SEEED-v1`  
**Hardware:** Seeed XIAO nRF52840 + Wio-SX1262 + capacitive rock probe on D0/A0 + SEN0171 PIR on D6 + optional HOBO MX2001/MX2201/MX2203  
**Build target:** `seeed_xiao_nrf52840_cca_mx_pir`  
**Meshtastic base:** `2.7.26`  
**CCA release:** `CCA-MX-PIR-ROCK-1.0.7`

This branch is the current CCA field-node build for the Navajo sandstone wetness experiment. It combines Meshtastic, the HOBO BLE bridge, CCA DM/power/PIR controls, persistent rock calibration, and compact rock telemetry for the Heltec V4 -> Vercel -> Neon pipeline.

## Current release identity

Expected rock-module startup log:

```text
CCA ROCK 1.0.7: D0/A0 sandstone + RF-filtered D6 PIR; 60 s telemetry; persistent calibration; temporary state bands; safe battery ADC
```

Current branch head before this documentation update was:

```text
3afaa3f Align CCA station version with 1.0.7 release
```

Settings-safe OTA release artifact name:

```text
CCA-CCS3-SETTINGS-SAFE-OTA-1.0.7
```

Application-only/settings-safe OTA preserves the filesystem, including saved rock calibration in `/prefs/cca_rock_cal.bin`.

## Wiring

| Device | XIAO |
|---|---|
| Rock/moisture SIG | **D0 / A0** |
| Rock/moisture VCC | **3V3** |
| Rock/moisture GND | **GND** |
| Unused Grove wire | disconnected |
| SEN0171 PIR signal | **D6** |
| SEN0171 VCC | **3V3** |
| SEN0171 GND | **GND** |

D6 is configured through the nRF52840's internal pulldown. No external pulldown is required by the current firmware, though hardware filtering can still be added if RF interference remains troublesome.

## Rock telemetry

The node broadcasts a 16-byte `PRIVATE_APP` packet with magic `RK`, schema 1.

```text
0..1   'R','K'
2      schema 1
3      bit0 = current validated/RF-filtered motion state
4..5   averaged rock ADC, scaled 0..4095
6..7   probe output mV
8..11  validated PIR rising-edge count since boot
12..13 battery mV; 0 = unavailable/invalid
14     battery percent; 0 when battery voltage unavailable
15     calibrated wetness 0..100; 255 = calibration unavailable
```

Normal automatic rock telemetry is sent every **60 seconds**, with the first automatic packet about two seconds after boot.

`ROCK NOW` forces an immediate rock packet.

### ADC handling

The XIAO battery subsystem expects the board's normal 10-bit ADC behavior. The rock module therefore:

1. reads A0 at native 10-bit resolution;
2. averages 20 samples;
3. scales the result mathematically to the established 0..4095 CCA rock scale.

Do **not** reintroduce a global `analogReadResolution(12)` call. That previously made battery readings roughly four times too high.

Sensor millivolts are derived from the 0..4095 rock value using the 3.3 V reference.

## PIR RF filtering

The SEN0171/D6 input can false-trigger from this node's own LoRa transmissions. Current firmware uses an RF-aware guard:

```text
CCA_PIR_POST_TX_GUARD_MS = 15000
```

Behavior:

- D6 uses `INPUT_PULLDOWN`;
- a NEW HIGH beginning while the node is transmitting is suppressed;
- a NEW HIGH beginning within 15 seconds after an observed local TX is suppressed;
- once a HIGH pulse is classified as RF-correlated, that whole HIGH remains suppressed until raw D6 returns LOW;
- a legitimate PIR HIGH that began before TX remains valid.

Rock telemetry does **not** transmit on PIR edges. This is intentional: PIR-triggered rock transmissions were removed because additional LoRa TX made the RF-feedback problem worse.

The rock module polls the validated PIR signal every 100 ms and increments the motion counter on validated LOW -> HIGH transitions.

Because the SEN0171 itself holds HIGH for many seconds and LoRa guard windows can overlap, use the PIR as a **coarse activity/visit sensor**, not an exact people counter.

## Persistent rock calibration

Calibration is stored in:

```text
/prefs/cca_rock_cal.bin
```

Record magic:

```text
RKC1
```

A valid calibrated wetness percentage requires both dry and wet endpoints and at least 20 ADC counts of separation. Either ADC direction is supported by the math.

### Calibration commands

```text
ROCK CAL DRY
ROCK CAL WET
ROCK CAL STATUS
ROCK CAL CLEAR
```

`ROCK CAL DRY` saves the current installed-rock ADC as the dry endpoint.

`ROCK CAL WET` saves the current ADC as the wet endpoint. Do not use it merely because a certain amount of rain has fallen; use it only when the probe location represents the wet endpoint you actually want to define.

`ROCK CAL STATUS` shows saved endpoints, current ADC, current temporary state, and calibrated wetness if available.

`ROCK CAL CLEAR` erases the saved endpoints.

## Firmware temporary state bands

The current **device-side** `ROCK STATE` and `ROCK BANDS` commands still use these legacy temporary bands:

```text
DRY      >=2303
DRYING   2000-2302
DAMP     1713-1999
WET      1600-1712
SOAKED   1451-1599
WATER    <=1450
```

These firmware labels are temporary and are separate from persistent dry/wet calibration.

### Current Vercel sandstone bands

The web dashboard was recalibrated during the 2026-08-26 Navajo sandstone test and now interprets raw ADC as:

```text
DRY      >=2300
DAMP     1850-2299
WET      1700-1849
SOAKED   <=1699
```

The web dashboard also adds direction labels:

```text
falling ADC over the recent trend window -> WETTING
rising ADC over the recent trend window  -> DRYING
```

Therefore the firmware text label and Vercel label can currently differ for the same ADC. **Raw ADC is the primary measurement; the Vercel bands above are the current sandstone interpretation.**

A future firmware release can synchronize `ROCK STATE`/`ROCK BANDS` with these field-derived bands if desired.

## 2026-08-26 sensor references

Earlier sensor checks established the expected direction:

| Condition | Approx. ADC |
|---|---:|
| Air | ~2338 |
| Really dry soil | ~2303 |
| Dry / slightly damp soil | ~1999 |
| Wet soil | ~1712 |
| Pure water | ~1386 |

Higher ADC = drier. Lower ADC = wetter.

### Navajo sandstone sprinkler test

```text
Sprinkler ON: 19:27 MDT
0.5 inch:     19:36 MDT
1.0 inch:     19:47 MDT
Water OFF:    19:49 MDT
```

The embedded probe dropped from roughly the low 2300s into the 2200s as the sandstone wetted, confirming a measurable response in the installed rock configuration.

## Complete DM command set

Commands are case-insensitive. A leading `/` is optional.

### General

```text
HELP
VERSION
STATUS
UPTIME
BOOT
DEBUG ON
DEBUG OFF
```

### Alert destination

```text
ALERTS HERE
ALERTS STATUS
ALERTS CLEAR
```

`ALERTS HERE` saves the sender as the destination for automatic private CCA alerts.

### PIR

```text
PIR
PIR STATUS
PIR COUNT
PIR LAST
PIR RESET
PIR ON
PIR OFF
PIR TX ON
PIR TX OFF
```

### Power

```text
POWER
POWER STATUS
POWER VOLTAGE
POWER MINMAX
POWER TREND
POWER HISTORY
POWER RESET
POWER UPTIME
```

### Rock

```text
ROCK
ROCK STATUS
ROCK ADC
ROCK STATE
ROCK NOW
ROCK BANDS
ROCK CAL
ROCK CAL DRY
ROCK CAL WET
ROCK CAL STATUS
ROCK CAL CLEAR
ROCK HELP
```

### HOBO logger

```text
LOGGER
READ
LOCK
UNLOCK
```

`LOCK` saves the currently identified **HOBO BLE MAC**. It does not restrict which Meshtastic nodes the Heltec gateway or cloud accepts.

### HELP response pages

Bare `HELP` returns five pages:

```text
HELP 1/5 GENERAL
HELP | VERSION | STATUS | UPTIME | BOOT
DEBUG ON | DEBUG OFF
ALERTS HERE | ALERTS STATUS | ALERTS CLEAR
```

```text
HELP 2/5 PIR
PIR | PIR STATUS | PIR COUNT | PIR LAST | PIR RESET
PIR ON | PIR OFF | PIR TX ON | PIR TX OFF
```

```text
HELP 3/5 POWER
POWER | POWER STATUS | POWER VOLTAGE | POWER MINMAX
POWER TREND | POWER HISTORY | POWER RESET | POWER UPTIME
```

```text
HELP 4/5 ROCK
ROCK | ROCK STATUS | ROCK ADC | ROCK STATE | ROCK NOW | ROCK BANDS
ROCK CAL | ROCK CAL DRY | ROCK CAL WET | ROCK CAL STATUS | ROCK CAL CLEAR | ROCK HELP
```

```text
HELP 5/5 LOGGER
LOGGER | READ | LOCK | UNLOCK
Commands are case-insensitive; leading / is optional.
```

## Cloud pipeline

```text
Seeed field node
  -> Meshtastic mesh
  -> Heltec V4 internet gateway
  -> Vercel /api/ingest
  -> Neon telemetry_readings
  -> Vercel dashboard
```

The CCS3 radio can continue sending about every minute, but the production cloud currently retains only **one rock record and one MX2201/environment record per 5-minute bucket**. Existing higher-frequency records from before the change remain in Neon.

Experiment-source filtering accepts `telemetry`, `rock`, `rock_test`, `sandstone`, and `motion` only from CCS3 (`node_num 1527161333`). `mx2001` is deliberately exempt so the established water-level pipeline is preserved.

Production dashboard:

```text
https://meshtastic-ecru.vercel.app
```

## Build / flash

From a local checkout of this repository:

```powershell
git fetch origin
git switch CCA-MX-HOBO-PIR-ROCK-SEEED-v1
git pull origin CCA-MX-HOBO-PIR-ROCK-SEEED-v1
```

Build:

```powershell
py -m platformio run -e seeed_xiao_nrf52840_cca_mx_pir
```

USB upload when the XIAO is available to PlatformIO:

```powershell
py -m platformio run -e seeed_xiao_nrf52840_cca_mx_pir -t upload
```

Serial monitor:

```powershell
py -m platformio device monitor -b 115200
```

After flashing, useful verification DMs are:

```text
VERSION
HELP
ROCK STATE
ROCK CAL STATUS
```

## Battery validation

Rock telemetry accepts battery voltage only in the physically reasonable field-node range:

```text
2500 mV to 5000 mV
```

Invalid historical values are treated as unavailable. The production dashboard also rejects impossible historical battery values rather than displaying them as real measurements.
