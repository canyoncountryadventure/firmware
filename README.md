# CCA MX + HOBO + PIR — Seeed XIAO

> **USE THIS BRANCH:** `CCA-MX-HOBO-PIR-SEEED-v1`
>
> **Hardware:** Seeed XIAO nRF52840 + Wio-SX1262 + DFRobot SEN0171 PIR + optional HOBO MX2001/MX2201/MX2203
>
> **Firmware:** `CCA-MX-PIR 1.0.1` · Schema `1` · Meshtastic `2.7.26`
>
> **Build target:** `seeed_xiao_nrf52840_cca_mx_pir`
>
> **PIR SIGNAL = D6. DO NOT PUT THE PIR SIGNAL ON D0 WITH THIS BUILD.**
>
> **D0/A0 is reserved for the future SEN0308 soil-moisture sensor. Soil is NOT in v1.**
>
> **PRIVACY FIX IN 1.0.1:** automatic PIR / power / boot alerts are PRIVATE DMs only. They never fall back to public LongFast broadcast.

For the detailed guide, open **[`START_HERE_CCA_MX_PIR.md`](START_HERE_CCA_MX_PIR.md)**.

---

## Current status

- GitHub Actions compile for 1.0.0: **PASS**
- Physical Seeed boot: **PASS**
- Wio-SX1262 radio initialization: **PASS**
- SEN0171 on D6: **PASS**
- PIR LOW→HIGH detection/counting: **PASS**
- Direct-message CCA commands: **PASS**
- PKI-encrypted DMs: **PASS**
- Existing universal HOBO module still initializes/scans/responds: **PASS**
- Power-status commands: **PASS**
- 1.0.1 private-alert change: **source updated; CI compile must pass before flashing**

**Actual Seeed/Wio-SX1262 TX power is 22 dBm**, even if the stored Meshtastic setting says 30 dBm.

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

# 2. VERY IMPORTANT — private automatic alerts

In firmware **1.0.1**, PIR, low-battery, recovery, and boot alerts are never broadcast to LongFast.

The receiving radio must claim the node once by sending this as a **DM** to the CCA node:

```text
ALERTS HERE
```

The CCA node stores the sender's node number in flash and uses it as the private destination after reboot.

Check it with:

```text
ALERTS STATUS
```

If no alert destination is set, automatic CCA alerts are **suppressed**, not broadcast publicly.

To deliberately remove the destination, send this from the currently assigned receiving radio:

```text
ALERTS CLEAR
```

A different radio cannot overwrite an existing alert destination. Clear it from the current receiver first, then send `ALERTS HERE` from the new receiver.

Automatic private DM examples:

```text
PIR|ALERT|COUNT=17
SYS|BOOT|COUNT=7|FW=CCA-MX-PIR-1.0.1
POWER|LOW|V=3.590
POWER|CRITICAL|V=3.440
POWER|RECOVERED|V=3.670
```

---

# 3. How to text the node

Use **Direct Message (DM)** from another Meshtastic node/radio to this CCA node.

- CCA commands are accepted only when addressed directly to this node.
- Public/broadcast channel text is ignored by the CCA command handler.
- Commands are **case-insensitive**.
- A leading slash is optional.

## Complete DM command cheat sheet

### Alert routing / privacy

| Text this | What it does |
|---|---|
| `ALERTS HERE` | Saves the sending radio as the persistent private destination for automatic CCA alerts |
| `ALERTS STATUS` | Shows the saved private alert destination and confirms public fallback is disabled |
| `ALERTS CLEAR` | Clears the saved destination; only the currently assigned receiver may clear it |

### Overall/system

| Text this | What it does |
|---|---|
| `STATUS` | Overall CCA status, battery, PIR state/count, and private alert destination |
| `VERSION` | Firmware identity, schema, Meshtastic base, hardware target |
| `UPTIME` | Uptime + persistent boot count |
| `BOOT` | Persistent boot count + CCA firmware identity |
| `DEBUG ON` | Enables extra USB-serial CCA diagnostics until reboot |
| `DEBUG OFF` | Turns extra CCA serial diagnostics off |

### PIR / motion sensor

| Text this | What it does |
|---|---|
| `PIR` | Full PIR status |
| `PIR STATUS` | Same full PIR status, including private alert destination |
| `PIR COUNT` | Persistent total detections + detections since this boot |
| `PIR LAST` | Time since the last PIR detection during this boot |
| `PIR RESET` | Clears persistent PIR total and current-boot PIR count |
| `PIR ON` | Enables PIR monitoring; persists |
| `PIR OFF` | Disables PIR monitoring; persists |
| `PIR TX ON` | Enables automatic PIR DMs to the saved private alert destination; persists |
| `PIR TX OFF` | PIR still counts locally but sends no PIR alerts; persists |

### Power / solar diagnostic history

| Text this | What it does |
|---|---|
| `POWER` | Battery voltage, %, charging indication, trend, min/max, sample count |
| `POWER STATUS` | Same summary as `POWER` |
| `POWER VOLTAGE` | Current voltage, battery %, charging indication |
| `POWER MINMAX` | Minimum, maximum, and current voltage since boot / last reset |
| `POWER TREND` | Current, ~1 h, ~6 h, ~24 h references and trend |
| `POWER HISTORY` | Current, ~1 h, ~6 h, ~12 h, ~24 h plus min/max |
| `POWER RESET` | Clears RAM-only power history/min/max; does not reboot or erase PIR counts |
| `POWER UPTIME` | Uptime + persistent boot count |

### HOBO — existing universal commands

| Text this | What it does |
|---|---|
| `LOGGER` | HOBO connection/model/MAC/BLE information, logging interval, lock state |
| `READ` | Immediate fresh HOBO reading without disturbing automatic schedule |
| `LOCK` | Saves the currently identified HOBO BLE MAC |
| `UNLOCK` | Clears the saved logger assignment and resumes discovery |

Supported HOBO families remain **MX2001, MX2201, and MX2203**.

---

# 4. Windows — correct branch

Local repo:

```text
C:\Meshtastic\HOBO\firmware
```

```powershell
cd C:\Meshtastic\HOBO\firmware
git fetch origin
git switch CCA-MX-HOBO-PIR-SEEED-v1
git pull --ff-only origin CCA-MX-HOBO-PIR-SEEED-v1
git branch --show-current
```

It must print:

```text
CCA-MX-HOBO-PIR-SEEED-v1
```

---

# 5. Build / flash / monitor

Build:

```powershell
.\CCA-MX-HOBO-PIR\build.ps1
```

Flash:

```powershell
.\CCA-MX-HOBO-PIR\flash.ps1
```

Serial monitor:

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
& $pio device monitor -b 115200
```

Expected 1.0.1 startup includes:

```text
CCA-MX-PIR 1.0.1: D6 PIR, schema 1
CCA automatic alerts: PRIVATE destination ...
```

---

# 6. First setup after flashing 1.0.1

From the radio that should receive the field node's automatic alerts, DM the CCA node:

```text
VERSION
ALERTS HERE
ALERTS STATUS
STATUS
LOGGER
PIR STATUS
POWER
```

Then trigger the PIR. The automatic `PIR|ALERT|COUNT=...` message should appear in the **DM conversation with that receiver**, not in LongFast.

Reboot and send `ALERTS STATUS` again to verify the destination survived.

---

# 7. Rollback / known-good HOBO reference

Untouched known-good universal HOBO branch:

```text
hobo-mx2001-mx2201-mx2203
```

```powershell
git fetch origin
git switch hobo-mx2001-mx2201-mx2203
git pull --ff-only origin hobo-mx2001-mx2201-mx2203
```
