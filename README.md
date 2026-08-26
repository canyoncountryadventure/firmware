# CCA MX + HOBO + PIR — Seeed XIAO

> **USE THIS BRANCH:** `CCA-MX-HOBO-PIR-SEEED-v1`
>
> **Hardware:** Seeed XIAO nRF52840 + Wio-SX1262 + DFRobot SEN0171 PIR + optional HOBO MX2001/MX2201/MX2203
>
> **Firmware:** `CCA-MX-PIR 1.0.0` · Schema `1` · Meshtastic `2.7.26`
>
> **Build target:** `seeed_xiao_nrf52840_cca_mx_pir`
>
> **PIR SIGNAL = D6. DO NOT PUT THE PIR SIGNAL ON D0 WITH THIS BUILD.**
>
> **D0/A0 is intentionally reserved for the future SEN0308 soil-moisture sensor. Soil is NOT in v1.**

For the detailed explanation, wiring, packet formats, persistence behavior, power logic, test plan, and rollback notes, open:

**[`START_HERE_CCA_MX_PIR.md`](START_HERE_CCA_MX_PIR.md)**

---

## Current status

- GitHub Actions compile: **PASS**
- Physical Seeed boot: **PASS**
- Wio-SX1262 radio initialization: **PASS**
- SEN0171 on D6: **PASS**
- PIR LOW→HIGH detection/counting: **PASS**
- Immediate PIR mesh transmission: **PASS**
- Direct-message CCA commands: **PASS**
- PKI-encrypted DMs: **PASS**
- Existing universal HOBO module still initializes/scans/responds: **PASS**
- Power-status commands: **PASS**
- Persistent PIR/boot values after a deliberate power-cycle: **still to verify on bench**

The physical bench log showed the PIR reaching `CCA PIR ALERT total=1` and then `total=2`, with both packets entering the LoRa mesh while the universal HOBO module remained active.

**Important radio note:** even if the stored Meshtastic setting says 30 dBm, this Seeed/Wio-SX1262 build is hardware-limited by firmware to **22 dBm actual TX power**.

---

# 1. Wiring

| SEN0171 | XIAO |
|---|---|
| VCC | 3V3 |
| GND | GND |
| Digital signal | **D6** |

If your PIR signal already has the 100 kΩ pull-down to GND from bench testing, keep it when moving the signal wire to D6.

### Pin plan

| XIAO pin | Use in CCA v1 |
|---|---|
| **D0 / A0** | **Reserved for future SEN0308 soil sensor; unused now** |
| D1 | Wio-SX1262 DIO1 |
| D2 | Wio-SX1262 RESET |
| D3 | Wio-SX1262 BUSY |
| D4 | Wio-SX1262 CS |
| D5 | Wio-SX1262 RX enable |
| **D6** | **SEN0171 PIR signal** |
| D7 | Free in this build |
| D8 | Wio-SX1262 SCK |
| D9 | Wio-SX1262 MISO |
| D10 | Wio-SX1262 MOSI |

GNSS is compiled out of this dedicated CCA target so D6 cannot be taken by the normal Seeed GNSS UART mapping.

---

# 2. How to text the node

Use **Direct Message (DM)** from another Meshtastic node/radio to this CCA node.

- CCA commands are accepted only when addressed directly to this node.
- Public/broadcast channel text is ignored by the CCA command handler.
- Command matching is **case-insensitive**: `power`, `Power`, and `POWER` are equivalent.
- A leading slash is optional: `STATUS` and `/STATUS` are equivalent.
- Extra leading/trailing spaces are ignored.

## Complete DM command cheat sheet

### Overall/system

| Text this | What it does |
|---|---|
| `STATUS` | Overall CCA status: firmware, uptime, battery, charging indication, PIR state/count; points to `LOGGER` for HOBO detail |
| `VERSION` | Firmware identity, schema, Meshtastic base, hardware target |
| `UPTIME` | Uptime + persistent boot count |
| `BOOT` | Persistent boot count + CCA firmware identity |
| `DEBUG ON` | Enables extra **USB serial** CCA diagnostics until reboot; does not change PIR/mesh behavior |
| `DEBUG OFF` | Turns the extra CCA serial diagnostics back off |

### PIR / motion sensor

| Text this | What it does |
|---|---|
| `PIR` | Full PIR status |
| `PIR STATUS` | Same full PIR status |
| `PIR COUNT` | Persistent total detections + detections since this boot |
| `PIR LAST` | Time since the last PIR detection during this boot |
| `PIR RESET` | Clears persistent PIR total and current-boot PIR count |
| `PIR ON` | Enables PIR monitoring; setting persists after reboot |
| `PIR OFF` | Disables PIR monitoring; setting persists after reboot |
| `PIR TX ON` | Every new PIR event transmits immediately; setting persists |
| `PIR TX OFF` | PIR still counts locally but does not transmit PIR alerts; setting persists |

### Power / solar diagnostic history

| Text this | What it does |
|---|---|
| `POWER` | Battery voltage, Meshtastic %, charging indication, trend, min/max, sample count |
| `POWER STATUS` | Same summary as `POWER` |
| `POWER VOLTAGE` | Current voltage, battery %, charging indication |
| `POWER MINMAX` | Minimum, maximum, and current voltage since boot / last reset |
| `POWER TREND` | Current, ~1 h, ~6 h, ~24 h references and trend |
| `POWER HISTORY` | Current, ~1 h, ~6 h, ~12 h, ~24 h plus min/max |
| `POWER RESET` | Clears **RAM-only power history/min/max** and starts a new history at the current voltage; does NOT reboot or erase PIR counts |
| `POWER UPTIME` | Uptime + persistent boot count |

### HOBO — existing universal firmware commands

| Text this | What it does |
|---|---|
| `LOGGER` | Shows HOBO connection/model/MAC/BLE information, logging interval, and lock state |
| `READ` | Performs an immediate fresh HOBO reading without disturbing the automatic record-aligned schedule |
| `LOCK` | Saves the currently identified HOBO BLE MAC and reconnects only to it after reboot |
| `UNLOCK` | Clears the saved logger assignment and resumes discovery of supported HOBO loggers |

Supported HOBO families in this branch remain **MX2001, MX2201, and MX2203**.

---

# 3. Automatic messages you do NOT have to request

The CCA node sends these automatically over normal Meshtastic text packets:

```text
PIR|ALERT|COUNT=17
SYS|BOOT|COUNT=7|FW=CCA-MX-PIR-1.0.0
POWER|LOW|V=3.590
POWER|CRITICAL|V=3.440
POWER|RECOVERED|V=3.670
```

- PIR alert: immediately on each new **LOW→HIGH** edge, provided `PIR TX ON`.
- Boot message: once, about 30 seconds after boot.
- Power alerts: only on threshold/state changes, not every power sample.
- HOBO automatic telemetry remains the existing model-specific telemetry and stays aligned to confirmed new HOBO records.

The data path remains:

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

---

# 4. Windows — get onto the correct branch

Current local repo used for this build:

```text
C:\Meshtastic\HOBO\firmware
```

PowerShell:

```powershell
cd C:\Meshtastic\HOBO\firmware
git fetch origin
git switch CCA-MX-HOBO-PIR-SEEED-v1
git pull --ff-only origin CCA-MX-HOBO-PIR-SEEED-v1
git branch --show-current
```

The final command **must** print:

```text
CCA-MX-HOBO-PIR-SEEED-v1
```

---

# 5. Build

Preferred helper:

```powershell
.\CCA-MX-HOBO-PIR\build.ps1
```

Manual equivalent:

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
& $pio run -e seeed_xiao_nrf52840_cca_mx_pir
```

Do not accidentally build the ordinary `seeed_xiao_nrf52840_kit` environment when you want the PIR firmware.

---

# 6. Flash

```powershell
.\CCA-MX-HOBO-PIR\flash.ps1
```

Manual equivalent:

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
& $pio run -e seeed_xiao_nrf52840_cca_mx_pir -t upload
```

---

# 7. USB serial monitor

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
& $pio device monitor -b 115200
```

If nothing prints, press the XIAO reset button once. Exit with `Ctrl+C`.

Expected CCA startup lines include:

```text
CCA-MX-PIR 1.0.0: D6 PIR, schema 1
CCA PIR: ON TX=ON
```

---

# 8. Fast bench test

From another Meshtastic radio, DM the CCA node in this order:

```text
VERSION
STATUS
LOGGER
PIR STATUS
POWER
```

Then let the PIR return LOW and trigger it once. Expect:

```text
PIR|ALERT|COUNT=<number>
```

Then DM:

```text
PIR COUNT
PIR LAST
POWER HISTORY
```

For the next persistence test, note `PIR COUNT` and `BOOT`, power-cycle the node, then send them again. The total PIR count and boot count should survive; the power-history buffer intentionally starts over after reboot.

---

# 9. Rollback / known-good HOBO reference

The untouched known-good universal HOBO branch remains:

```text
hobo-mx2001-mx2201-mx2203
```

To return to it:

```powershell
git fetch origin
git switch hobo-mx2001-mx2201-mx2203
git pull --ff-only origin hobo-mx2001-mx2201-mx2203
```

Do not merge the CCA branch into that known-good HOBO reference merely to bench-test this node.
