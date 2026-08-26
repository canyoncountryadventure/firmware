# START HERE — CCA MX + HOBO + PIR (Seeed)

> **THIS IS THE BRANCH TO USE FOR THE SEEED XIAO nRF52840 + Wio-SX1262 + HOBO MX + SEN0171 PIR BUILD.**
>
> **Branch:** `CCA-MX-HOBO-PIR-SEEED-v1`
>
> **Build target:** `seeed_xiao_nrf52840_cca_mx_pir`
>
> **CCA firmware identity:** `CCA-MX-PIR 1.0.0` / schema `1`
>
> **Meshtastic base:** `2.7.26`
>
> **BUILD STATUS: VERIFIED — GitHub Actions compiled this exact nRF52840 target successfully on August 25, 2026 (Mountain Time).**

## IF YOU ONLY REMEMBER THREE THINGS

1. Open branch **`CCA-MX-HOBO-PIR-SEEED-v1`**.
2. Read **this file: `START_HERE_CCA_MX_PIR.md`**.
3. PIR signal is **D6**, not D0. D0/A0 is intentionally reserved for future soil moisture.

This branch is based directly on the proven `hobo-mx2001-mx2201-mx2203` branch. The existing universal HOBO MX2001/MX2201/MX2203 BLE reader, logger lock, manual `READ`, and automatic record-aligned HOBO telemetry are intentionally preserved.

The new CCA module adds the SEN0171 as a **repeater presence/tamper alarm**, plus battery/power history and remote health commands. It is **not** a trail counter.

---

## 1. Hardware for this build

- Seeed Studio XIAO nRF52840
- Wio-SX1262 for XIAO, standalone/kit pinout (SKU 102010710 / equivalent)
- DFRobot SEN0171 PIR motion sensor
- Existing battery / solar charger connection
- Optional HOBO MX2001, MX2201, or MX2203 over BLE

**Soil moisture is NOT implemented in v1.** D0/A0 is intentionally left available for a future SEN0308 soil-moisture input.

---

## 2. PIR wiring — IMPORTANT CHANGE

The old SEN0171 bench/trail test used D0. **Do not use D0 with this build. Move the PIR signal wire from D0 to D6.**

| SEN0171 | Seeed XIAO |
|---|---|
| VCC | 3V3 |
| GND | GND |
| Digital signal | **D6** |

If the existing bench wiring has the 100 kΩ pull-down from the PIR signal to GND, keep it with the signal after moving that signal to D6.

### Reserved pin map

| XIAO pin | CCA v1 use |
|---|---|
| **D0 / A0** | **Reserved for future SEN0308 soil probe — unused now** |
| D1 | Wio-SX1262 DIO1 |
| D2 | Wio-SX1262 RESET |
| D3 | Wio-SX1262 BUSY |
| D4 | Wio-SX1262 CS |
| D5 | Wio-SX1262 RX enable |
| **D6** | **SEN0171 PIR digital signal** |
| D7 | Free for this build |
| D8 | Wio-SX1262 SCK |
| D9 | Wio-SX1262 MISO |
| D10 | Wio-SX1262 MOSI |

### Why GNSS is disabled in this build

The normal Seeed kit maps the L76K GNSS UART TX function to D6. This dedicated CCA build compiles the Meshtastic GPS module out so D6 belongs to the PIR and cannot be taken over by GNSS firmware. This does **not** alter the normal Seeed build target or the original HOBO branch.

The solar charger is power-only in v1. No charger status GPIO is required.

---

## 3. What stays unchanged from the HOBO firmware

The CCA PIR code runs beside the existing universal HOBO code. It does not replace or disable it.

Existing HOBO commands remain:

- `LOGGER` — connected logger/model/MAC/BLE/interval/lock information
- `READ` — immediate HOBO reading
- `LOCK` — lock the radio to the currently identified HOBO
- `UNLOCK` — clear the persistent logger lock

Existing automatic telemetry also remains unchanged:

- MX2001: existing compact `PRIVATE_APP` binary packet
- MX2201 / MX2203: existing Meshtastic environmental telemetry packet
- timing remains aligned to the HOBO logger's own new-record behavior

A HOBO BLE problem does not disable the CCA PIR thread.

---

## 4. PIR behavior

Default after first flash:

- PIR monitoring: **ON**
- PIR transmissions: **ON**
- input pin: **D6**
- poll interval: **100 ms**
- trigger: **LOW → HIGH only**
- no firmware cooldown
- re-arms when the SEN0171 output returns LOW
- a PIR that powers up HIGH is ignored until it returns LOW once, preventing a false startup alarm

Every valid detection is transmitted immediately as a normal Meshtastic text message on channel 0:

```text
PIR|ALERT|COUNT=17
```

The SEN0171 itself can remain HIGH for roughly 6–11 seconds. The firmware does not add any extra hold time; the next event can occur as soon as the sensor actually returns LOW and goes HIGH again.

### PIR persistence

- total detection count persists across reboot
- PIR ON/OFF persists across reboot
- PIR TX ON/OFF persists across reboot
- detections since boot reset naturally at reboot
- `PIR RESET` clears the persistent total and current boot count

Because a tamper event should be rare, the total is saved immediately after each detection so a later power cut does not erase the local total.

---

## 5. PIR commands

Send these as a **direct message to the node**. Broadcast channel messages are ignored by the CCA command handler.

| Command | Result |
|---|---|
| `PIR` | Full PIR status |
| `PIR STATUS` | Full PIR status |
| `PIR COUNT` | Persistent total + detections this boot |
| `PIR LAST` | Time since last detection this boot |
| `PIR RESET` | Reset PIR counters |
| `PIR ON` | Enable PIR monitoring; persists |
| `PIR OFF` | Disable PIR monitoring; persists |
| `PIR TX ON` | Transmit every new PIR detection; persists |
| `PIR TX OFF` | Count silently without transmitting; persists |

---

## 6. Power / solar diagnostics

The CCA module reads the same battery voltage and percentage Meshtastic already obtains from the XIAO battery-sense circuit. It does **not** claim to directly measure solar-panel voltage or charge current.

A battery sample is stored in RAM every **10 minutes**. Up to about 24 hours are retained.

Tracked values:

- current battery voltage
- Meshtastic battery percentage estimate
- whether the board reports charging
- minimum and maximum sampled voltage since boot / last `POWER RESET`
- approximately 1 h, 6 h, 12 h, and 24 h history
- rising / falling / stable 6-hour trend once enough history exists

Power history is diagnostic RAM state and starts fresh after reboot. PIR totals and boot count are persistent.

### Automatic power alerts

- below **3.60 V**: `POWER|LOW|V=3.590`
- below **3.45 V**: `POWER|CRITICAL|V=3.440`
- recovery uses a **3.65 V** hysteresis point to avoid repeated threshold chatter
- state-change alerts are broadcast as Meshtastic text; it does not spam every sample

---

## 7. Power commands

| Command | Result |
|---|---|
| `POWER` | Quick battery summary |
| `POWER STATUS` | Same diagnostic summary |
| `POWER VOLTAGE` | Current voltage, battery %, charging state |
| `POWER MINMAX` | Min / max / current voltage |
| `POWER TREND` | 1 h / 6 h / 24 h references + trend |
| `POWER HISTORY` | Now / 1 h / 6 h / 12 h / 24 h + min/max |
| `POWER RESET` | Clear in-RAM power history/min/max and start over |
| `POWER UPTIME` | Uptime and persistent boot count |

---

## 8. System commands

| Command | Result |
|---|---|
| `STATUS` | CCA firmware, uptime, battery, PIR summary; directs you to `LOGGER` for HOBO link detail |
| `VERSION` | CCA version, schema, Meshtastic base, hardware target |
| `UPTIME` | Uptime + persistent boot count |
| `BOOT` | Persistent boot count + CCA version |
| `DEBUG ON` | Extra CCA serial logging until reboot |
| `DEBUG OFF` | Disable extra CCA serial logging |

On every boot, after a 30-second startup delay, the node sends:

```text
SYS|BOOT|COUNT=7|FW=CCA-MX-PIR-1.0.0
```

That provides a remote clue if somebody disconnects/reconnects power or the node repeatedly resets.

---

## 9. Heltec → Neon → Vercel path

The radio side is intentionally simple:

```text
SEN0171 / HOBO
      ↓
CCA Seeed node
      ↓
Meshtastic mesh
      ↓
Heltec V4 gateway
      ↓
Neon
      ↓
Vercel
```

The existing HOBO packet formats are unchanged, so the existing Heltec/Neon path is not broken by this branch.

The **new** CCA messages are deliberately machine-readable text:

```text
PIR|ALERT|COUNT=17
POWER|LOW|V=3.590
POWER|CRITICAL|V=3.440
POWER|RECOVERED|V=3.670
SYS|BOOT|COUNT=7|FW=CCA-MX-PIR-1.0.0
```

The Heltec will receive these as normal Meshtastic text packets. A later Heltec parser update will be needed if you want these new PIR/power/system events automatically inserted into dedicated Neon fields/tables. That update is separate from this sensor-node firmware.

---

## 10. VERIFIED BUILD

GitHub Actions successfully compiled the exact target below on **August 25, 2026 (Mountain Time)**:

```text
seeed_xiao_nrf52840_cca_mx_pir
```

The build compiled both the existing universal HOBO module and the new `CCAStationModule`, linked successfully for the XIAO nRF52840, and generated flashable UF2/DFU artifacts.

Verified build artifact name:

```text
firmware-nrf52840-seeed_xiao_nrf52840_cca_mx_pir-CCA-MX-PIR-1.0.0
```

This means the combined firmware **fits the configured Seeed XIAO nRF52840 firmware region and builds successfully**. Bench testing is still required to validate real PIR wiring, mesh alerts, persistent counters, battery readings, and unchanged HOBO behavior on the physical node.

---

## 11. Build — Windows PowerShell

First make absolutely sure Git is on this branch:

```powershell
git fetch origin
git switch CCA-MX-HOBO-PIR-SEEED-v1
git pull
```

Confirm:

```powershell
git branch --show-current
```

It must print:

```text
CCA-MX-HOBO-PIR-SEEED-v1
```

Then build with the dedicated helper:

```powershell
.\CCA-MX-HOBO-PIR\build.ps1
```

The helper builds **only** this PlatformIO environment:

```text
seeed_xiao_nrf52840_cca_mx_pir
```

Manual equivalent:

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
& $pio run -e seeed_xiao_nrf52840_cca_mx_pir
```

---

## 12. Flash

With the Seeed connected by USB:

```powershell
.\CCA-MX-HOBO-PIR\flash.ps1
```

Manual equivalent:

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
& $pio run -e seeed_xiao_nrf52840_cca_mx_pir -t upload
```

Do **not** build or flash `seeed_xiao_nrf52840_kit` when you intend to test the PIR. That target is the normal Seeed firmware and does not define the CCA PIR module.

---

## 13. First bench test after flashing

1. Move the SEN0171 signal wire from D0 to **D6**.
2. Leave D0/A0 unused.
3. Flash the CCA target above.
4. Let the node boot for at least 30 seconds.
5. Direct-message the node: `VERSION`.
6. Expect `FW: CCA-MX-PIR 1.0.0`.
7. Send `LOGGER` and verify the existing HOBO support still reports normally.
8. Send `PIR STATUS` and verify `Pin: D6` and `TX Alerts: ON`.
9. Let PIR settle LOW, then walk in front of it.
10. Expect an immediate mesh message similar to `PIR|ALERT|COUNT=1`.
11. Send `POWER` and `POWER HISTORY`.
12. Reboot the node and send `BOOT` / `PIR COUNT` to verify persistent boot and PIR totals.

---

## 14. Rollback / known-good reference

If the CCA build needs to be abandoned during testing, the untouched known-good HOBO branch is:

```text
hobo-mx2001-mx2201-mx2203
```

That is the reference branch for the existing universal HOBO behavior.

---

## 15. Source files added/changed for CCA v1

CCA-specific code:

```text
src/modules/CCAStationModule.h
src/modules/CCAStationModule.cpp
CCA-MX-HOBO-PIR/build.ps1
CCA-MX-HOBO-PIR/flash.ps1
START_HERE_CCA_MX_PIR.md
```

Integration points:

```text
src/modules/Modules.cpp
variants/nrf52840/seeed_xiao_nrf52840_kit/platformio.ini
```

The large existing universal HOBO implementation is not being rewritten for PIR. This keeps the proven MX2001/MX2201/MX2203 logic isolated from the new CCA security/power module.
