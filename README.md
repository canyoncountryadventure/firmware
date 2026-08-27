# CCA MX + HOBO + PIR + Rock — Seeed XIAO

> **USE THIS BRANCH:** `CCA-MX-HOBO-PIR-ROCK-SEEED-v1`
>
> **Release:** `CCA-MX-PIR-ROCK 1.0.7` · Meshtastic `2.7.26`
>
> **Hardware:** Seeed XIAO nRF52840 + Wio-SX1262 + capacitive rock/soil probe on D0/A0 + SEN0171 PIR on D6 + optional HOBO MX2001/MX2201/MX2203
>
> **Build target:** `seeed_xiao_nrf52840_cca_mx_pir`
>
> **PIR:** RF-filtered D6 with 15-second post-TX guard
>
> **Automatic CCA text alerts:** private DM only; no public LongFast fallback

This is the current combined CCA field-node branch for sandstone/soil wetness + PIR + HOBO monitoring.

---

## 1. Current validated release

Expected startup identity:

```text
CCA-MX-PIR 1.0.7
CCA ROCK 1.0.7: D0/A0 sandstone + RF-filtered D6 PIR; 60 s telemetry; persistent calibration; temporary state bands; safe battery ADC
```

GitHub Actions has successfully compiled the exact 1.0.7 rock branch.

Settings-safe OTA release artifact:

```text
CCA-CCS3-SETTINGS-SAFE-OTA-1.0.7
```

Application-only/settings-safe OTA preserves the filesystem, including saved rock calibration in:

```text
/prefs/cca_rock_cal.bin
```

---

## 2. Wiring

| Device | XIAO |
|---|---|
| Rock/moisture SIG | **D0 / A0** |
| Rock/moisture VCC | **3V3** |
| Rock/moisture GND | **GND** |
| Unused Grove wire | disconnected |
| SEN0171 PIR signal | **D6** |
| SEN0171 VCC | **3V3** |
| SEN0171 GND | **GND** |

Wio-SX1262 uses D1-D5 and D8-D10. D7 remains free.

D6 is configured with the nRF52840 internal pulldown. An external pull-down is not required by the current firmware, although hardware filtering can still be added if field RF conditions warrant it.

---

## 3. Fatal PIR bug and final fix

Soil/rock bench testing exposed a self-trigger failure: the node's own LoRa TX could drive the SEN0171/D6 input HIGH. Because the PIR can hold HIGH for several seconds, a normal edge detector could count that RF-induced pulse as motion and transmit again, creating a feedback loop.

Current firmware uses the shared RF-aware filter in `CCAStationModule.h`:

```text
CCA_PIR_POST_TX_GUARD_MS = 15000
```

Rules:

- D6 uses `INPUT_PULLDOWN`;
- a **new** raw HIGH beginning while local LoRa TX is active is rejected;
- a new raw HIGH beginning within **15 seconds after observed local TX** is rejected;
- once a HIGH is classified as RF-correlated, the **entire physical HIGH pulse** remains rejected until raw D6 returns LOW;
- only a fresh LOW→HIGH after physical LOW can count as motion;
- a legitimate PIR HIGH that began before a later TX remains valid.

The rock module includes the same CCA PIR header, so its motion flag and motion counter use the **same filtered D6 signal** as the CCA PIR alert logic.

### Important regression rule

Rock telemetry does **not** transmit because of PIR edges. An earlier PIR-edge transmit experiment was removed because extra LoRa transmissions worsened the RF-feedback problem.

The PIR should be treated as a coarse visit/presence/tamper sensor, not an exact people counter.

---

## 4. Rock telemetry packet

Automatic rock telemetry is a 16-byte `PRIVATE_APP` packet with magic `RK`, schema 1:

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

Normal automatic interval:

```text
60 seconds
```

First automatic packet is approximately two seconds after boot. `ROCK NOW` forces an immediate packet.

---

## 5. ADC and battery handling

The XIAO battery subsystem expects the board's normal 10-bit ADC behavior. The rock module therefore:

1. reads A0 at native 10-bit resolution;
2. averages 20 samples;
3. mathematically scales the average to the established 0..4095 CCA rock scale.

Do **not** reintroduce a global:

```text
analogReadResolution(12)
```

That previously made battery voltage readings approximately four times too high.

Rock telemetry accepts battery voltage only in the physically reasonable field-node range:

```text
2500 mV to 5000 mV
```

Invalid values are reported as unavailable rather than treated as real battery measurements.

---

## 6. Persistent rock calibration

Calibration file:

```text
/prefs/cca_rock_cal.bin
```

Record magic:

```text
RKC1
```

Commands:

```text
ROCK CAL
ROCK CAL DRY
ROCK CAL WET
ROCK CAL STATUS
ROCK CAL CLEAR
```

A valid calibrated wetness percentage requires both dry and wet endpoints and at least 20 ADC counts of separation. Either ADC direction is supported.

`ROCK CAL DRY` stores the current installed-rock ADC as the dry endpoint.

`ROCK CAL WET` stores the current ADC as the wet endpoint. Use it only when the probe location represents the wet endpoint you actually intend to define; do not equate a fixed rainfall amount with the wet endpoint automatically.

---

## 7. Current rock-state interpretation

### Device-side temporary bands

Firmware `ROCK STATE` / `ROCK BANDS` currently use:

```text
DRY      >=2303
DRYING   2000-2302
DAMP     1713-1999
WET      1600-1712
SOAKED   1451-1599
WATER    <=1450
```

These are temporary firmware labels and are separate from persistent dry/wet calibration.

### Current sandstone dashboard bands

The dashboard interpretation derived during the Navajo sandstone test is:

```text
DRY      >=2300
DAMP     1850-2299
WET      1700-1849
SOAKED   <=1699
```

Trend direction:

```text
falling ADC -> WETTING
rising ADC  -> DRYING
```

Raw ADC remains the primary measurement. Firmware and dashboard labels can differ until a future firmware release intentionally synchronizes the bands.

### Reference measurements

| Condition | Approx. ADC |
|---|---:|
| Air | ~2338 |
| Really dry soil | ~2303 |
| Dry / slightly damp soil | ~1999 |
| Wet soil | ~1712 |
| Pure water | ~1386 |

Higher ADC = drier. Lower ADC = wetter.

---

## 8. Complete DM command set

Commands are case-insensitive. A leading `/` is optional. Send commands as direct messages to the field node.

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

### Private alert destination

```text
ALERTS HERE
ALERTS STATUS
ALERTS CLEAR
```

`ALERTS HERE` saves the sender as the persistent destination for automatic CCA PIR/power/boot text alerts. If no destination is configured, automatic CCA text alerts are suppressed; they do not fall back to public LongFast.

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

### Rock / soil

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

### HOBO

```text
LOGGER
READ
LOCK
UNLOCK
```

`LOCK` persists the identified HOBO BLE MAC. It does not restrict Meshtastic gateway/cloud node acceptance.

Bare `HELP` returns five pages covering General, PIR, Power, Rock, and Logger commands.

---

## 9. Data path

Authoritative system direction:

```text
Rock probe / PIR / HOBO
        ↓
CCA Seeed field node
        ↓
Meshtastic mesh
        ↓
Heltec V4 internet gateway
        ↓
Neon
        ↓
Vercel dashboard
```

The field node can transmit rock telemetry every minute. Cloud-side retention/downsampling may store a lower cadence; that does not change the radio's sampling/transmit behavior.

---

## 10. Build / flash

```powershell
git fetch origin
git switch CCA-MX-HOBO-PIR-ROCK-SEEED-v1
git pull --ff-only origin CCA-MX-HOBO-PIR-ROCK-SEEED-v1
```

Build:

```powershell
py -m platformio run -e seeed_xiao_nrf52840_cca_mx_pir
```

USB upload:

```powershell
py -m platformio run -e seeed_xiao_nrf52840_cca_mx_pir -t upload
```

Serial monitor:

```powershell
py -m platformio device monitor -b 115200
```

Useful post-flash DMs:

```text
VERSION
HELP
ALERTS STATUS
PIR STATUS
ROCK STATE
ROCK CAL STATUS
LOGGER
```

---

## 11. Required PIR regression test

After any future PIR-related change:

1. leave the unit motionless and verify no spontaneous counts;
2. trigger one real motion event and verify exactly one valid count/alert;
3. with nobody moving, repeatedly DM commands so the node transmits LoRa replies;
4. continue observing through the 15-second guard and physical PIR hold;
5. verify neither `PIR COUNT` nor the rock motion count increases from self-TX;
6. after raw D6 returns LOW, verify real motion can be detected again.

Do not call a future PIR build deployable until this self-TX test passes.
