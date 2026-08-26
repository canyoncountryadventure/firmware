# CCA MX + HOBO + PIR + Rock — Seeed XIAO

**Branch:** `CCA-MX-HOBO-PIR-ROCK-SEEED-v1`  
**Hardware:** Seeed XIAO nRF52840 + Wio-SX1262 + Grove capacitive moisture probe on D0/A0 + SEN0171 PIR on D6 + optional HOBO MX2001/MX2201/MX2203  
**Build target:** `seeed_xiao_nrf52840_cca_mx_pir`  
**Meshtastic base:** `2.7.26`

This branch is the CCA field-node build used for the Navajo sandstone wetness experiment. It preserves the HOBO BLE bridge and CCA DM/power/PIR commands while adding a compact rock/moisture packet for the Heltec -> Vercel -> Neon pipeline.

## Wiring

| Device | XIAO |
|---|---|
| Grove moisture SIG (yellow) | **D0 / A0** |
| Grove VCC (red) | **3V3** |
| Grove GND (black) | **GND** |
| Grove white | disconnected |
| SEN0171 PIR signal | **D6** |
| SEN0171 VCC | **3V3** |
| SEN0171 GND | **GND** |

No pull-down resistor is required in the current D6 PIR wiring.

## Rock telemetry

The node broadcasts a 16-byte `PRIVATE_APP` packet with magic `RK`, schema 1.

```text
0..1   'R','K'
2      schema 1
3      bit0 = current PIR motion state
4..5   rock ADC, 0..4095 CCA calibration scale
6..7   probe output mV
8..11  motion rising-edge count since boot
12..13 battery mV; 0 = unavailable/invalid
14     battery percent; 0 when battery voltage unavailable
15     reserved
```

Normal rock telemetry is sent every 60 seconds. Starting with the 2026-08-26 motion update, an additional RK packet is sent immediately on both PIR transitions:

- LOW -> HIGH: motion detected and counter increments;
- HIGH -> LOW: motion clear.

The module polls D6 every 100 ms, so the cloud can now timestamp Last Motion and Last Clear to roughly the PIR poll interval rather than waiting for the next 60-second sample.

Relevant fixes:

- `501bd91d` — initial battery/ADC fix;
- `1021a453` — eliminate ADC-resolution race while preserving the established 0..4095 rock scale;
- `dc398892` — immediate RK telemetry on PIR detect/clear transitions.

Expected serial identity from the rock module:

```text
CCA ROCK 1.0.3: D0/A0 sandstone + D6 PIR; 60 s + PIR edge telemetry; safe battery ADC
```

## Temporary moisture calibration

Until the Navajo sandstone drill-hole calibration is complete, the dashboard uses the earlier bench/soil observations as a temporary relative scale:

| Condition | Approx. ADC |
|---|---:|
| Really dry | 2303 |
| Dry / slightly damp | 1999 |
| Wet soil | 1712 |
| Pure water | 1386 |

Temporary dashboard classes:

```text
DRY      >= 2303
DRYING   2000–2302
DAMP     1713–1999
WET      1387–1712
WATER    <= 1386
```

Temporary relative wetness index:

```text
0%   = ADC 2303
100% = ADC 1386
```

This is **not volumetric water content** and is **not yet a sandstone climb/no-climb threshold**. Replace these temporary values after the actual Navajo sandstone wet/dry test identifies the site-specific climbable ADC.

## Validated water response — 2026-08-26

Before immersion, CCS3 was stable around ADC 2328–2351 / about 1.88–1.90 V. After the probe was put in water it dropped to:

```text
ADC 1407 / 1.134 V
ADC 1407 / 1.134 V
ADC 1406 / 1.133 V
```

The radio -> Heltec -> Vercel -> Neon path preserved those values correctly.

## Battery bug and fix

The first rock build showed impossible battery readings around 11–16 V / 100%.

Root cause: the rock module called `analogReadResolution(12)` globally. The XIAO battery circuit is configured by the board variant for **10-bit battery sensing** (`BATTERY_SENSE_RESOLUTION_BITS = 10`). The battery calculation therefore interpreted a 12-bit ADC result as though it were 10-bit, producing roughly a 4x voltage error.

The current fix never changes the MCU ADC away from the board's normal 10-bit setting. The rock probe is sampled at native 10-bit resolution, averaged, then mathematically scaled to the existing 0..4095 CCA calibration scale. This preserves prior rock ADC thresholds without interfering with battery sensing.

Rock telemetry also rejects battery voltages outside 2.5–5.0 V. Invalid battery data is sent as unavailable rather than displaying an impossible value.

**A Seeed firmware reflash is required for the battery fix.** Historical 11–16 V rows in Neon remain historical bad data; the dashboard hides them.

## `LOCK` does NOT lock the cloud/database source

The existing HOBO commands are:

| DM | Purpose |
|---|---|
| `LOGGER` | Show current HOBO/logger state |
| `READ` | Force a fresh HOBO reading |
| `LOCK` | Save the currently identified **HOBO BLE MAC** |
| `UNLOCK` | Clear the saved HOBO assignment and resume discovery |

Therefore, `LOCK` does **not** tell the Heltec, Vercel, or Neon to accept only this Meshtastic node.

For the sandstone experiment, source authorization is enforced at the Vercel ingest layer. `telemetry`, `rock`, `rock_test`, `sandstone`, and `motion` are accepted only from CCS3 (`node_num 1527161333`). `mx2001` is deliberately exempt so the established water-level monitoring pipeline continues to work.

## Other useful CCA DM commands

```text
VERSION
STATUS
UPTIME
BOOT
LOGGER
READ
LOCK
UNLOCK
PIR
PIR STATUS
PIR COUNT
PIR LAST
PIR RESET
PIR ON
PIR OFF
PIR TX ON
PIR TX OFF
ALERTS HERE
ALERTS STATUS
ALERTS CLEAR
POWER
POWER STATUS
POWER VOLTAGE
POWER MINMAX
POWER TREND
POWER HISTORY
POWER RESET
POWER UPTIME
DEBUG ON
DEBUG OFF
```

`ALERTS HERE` controls the private destination for automatic CCA DM alerts. It is separate from database source filtering.

## Build on Windows

Local repo used during the 2026-08-26 test:

```text
C:\Meshtastic-HOBO\firmware
```

Update to the branch head:

```powershell
cd C:\Meshtastic-HOBO\firmware
git fetch cca CCA-MX-HOBO-PIR-ROCK-SEEED-v1
git switch --detach cca/CCA-MX-HOBO-PIR-ROCK-SEEED-v1
```

Build:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e seeed_xiao_nrf52840_cca_mx_pir
```

Locate the UF2:

```powershell
Get-ChildItem .\.pio\build\seeed_xiao_nrf52840_cca_mx_pir -Filter *.uf2
```

The XIAO can then be put in UF2 bootloader mode and the generated UF2 copied to its boot drive.

## Cloud/dashboard fixes from the same test

The dashboard previously converted JavaScript `null` to numeric zero (`Number(null) === 0`), creating fake values such as `ADC 0`, `0.0%`, and `32.0°F`. That has been fixed: missing values remain blank/`—`.

The dashboard now includes:

- current moisture class;
- rock ADC;
- temporary wetness index;
- probe output voltage;
- MX2201 temperature;
- Motion Now;
- Last Motion and age;
- Last Clear and age;
- motion count;
- validated node battery voltage/percent;
- LoRa RSSI/SNR/hops;
- experiment timeline and charts.

The production dashboard filters its display to CCS3, and the ingest API filters future experiment telemetry to CCS3 before database insertion.
