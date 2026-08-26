# START HERE — CCA MX + HOBO + PIR (Seeed)

> **AUTHORITATIVE BRANCH:** `CCA-MX-HOBO-PIR-SEEED-v1`
>
> **Build target:** `seeed_xiao_nrf52840_cca_mx_pir`
>
> **Firmware:** `CCA-MX-PIR 1.0.1`
>
> **Schema:** `1`
>
> **Meshtastic base:** `2.7.26`
>
> **PIR signal:** **D6**
>
> **D0/A0:** reserved for future SEN0308 soil moisture; soil is not implemented yet.
>
> **1.0.1 PRIVACY RULE:** automatic PIR, power, and boot alerts are private DMs only. There is no public LongFast fallback.

The root [`README.md`](README.md) is the fast command/reference sheet. This file is the field/build explanation.

---

## 1. What this branch contains

This branch keeps the known-good universal HOBO support and adds the CCA station functions beside it:

```text
Meshtastic 2.7.26
├── HOBO MX2001 / MX2201 / MX2203 BLE reader
├── SEN0171 PIR presence/tamper alarm on D6
├── battery/power history
├── boot/uptime diagnostics
└── remote DM commands
```

The PIR is being used as a **presence/tamper alarm**, not a high-speed trail counter.

The untouched HOBO reference branch remains:

```text
hobo-mx2001-mx2201-mx2203
```

---

## 2. Wiring

| SEN0171 | Seeed XIAO |
|---|---|
| VCC | 3V3 |
| GND | GND |
| Digital signal | **D6** |

If the PIR already has the 100 kΩ pull-down from its signal wire to GND, keep it.

### Exposed pin map

| XIAO pin | CCA use |
|---|---|
| **D0 / A0** | Reserved for future SEN0308 soil probe |
| D1 | Wio-SX1262 DIO1 |
| D2 | Wio-SX1262 RESET |
| D3 | Wio-SX1262 BUSY |
| D4 | Wio-SX1262 CS |
| D5 | Wio-SX1262 RX enable |
| **D6** | **SEN0171 PIR** |
| D7 | Free |
| D8 | Wio-SX1262 SCK |
| D9 | Wio-SX1262 MISO |
| D10 | Wio-SX1262 MOSI |

The dedicated CCA build compiles the normal Seeed GNSS module out so D6 cannot be taken by the L76K UART mapping.

---

## 3. PIR behavior

Defaults:

- monitoring ON
- PIR TX ON
- poll every 100 ms
- event = LOW → HIGH edge
- no extra firmware cooldown
- re-arm when the SEN0171 returns LOW
- if PIR is already HIGH at boot, ignore it until it returns LOW once

Each valid detection:

1. increments persistent total;
2. increments since-boot count;
3. stores last-detection uptime;
4. saves persistent state;
5. if `PIR TX ON` and a private alert destination is configured, sends:

```text
PIR|ALERT|COUNT=17
```

The SEN0171 itself can remain HIGH for roughly 6–11 seconds. The firmware does not extend that hardware hold.

---

## 4. PRIVATE automatic alerts — set this after flashing

Firmware 1.0.1 does **not** broadcast automatic CCA alerts onto LongFast.

From the radio that should receive the alerts, open a **direct-message conversation with the CCA field node** and send:

```text
ALERTS HERE
```

That saves the sending node number in flash.

Verify:

```text
ALERTS STATUS
```

Expected idea:

```text
Alerts: PRIVATE DM ONLY
Destination: !xxxxxxxx
Public fallback: DISABLED
```

Automatic private DMs include:

```text
PIR|ALERT|COUNT=17
SYS|BOOT|COUNT=7|FW=CCA-MX-PIR-1.0.1
POWER|LOW|V=3.590
POWER|CRITICAL|V=3.440
POWER|RECOVERED|V=3.670
```

If no destination is configured, these automatic messages are **suppressed** rather than sent publicly.

To change receivers:

1. From the currently assigned receiver, DM:

```text
ALERTS CLEAR
```

2. From the new receiver, DM:

```text
ALERTS HERE
```

A different node cannot silently overwrite an existing alert destination.

---

## 5. DM command rules

CCA commands:

- must be sent as a **DM to the CCA node**;
- are ignored if posted as ordinary broadcast-channel text;
- are case-insensitive;
- allow an optional leading `/`.

Examples that are equivalent:

```text
POWER
Power
power
/POWER
```

---

## 6. COMPLETE DM COMMAND LIST

### Alert routing

```text
ALERTS HERE
ALERTS STATUS
ALERTS CLEAR
```

- `ALERTS HERE` — assign this sending radio as the private automatic-alert destination.
- `ALERTS STATUS` — show the saved destination and confirm public fallback is disabled.
- `ALERTS CLEAR` — remove the saved destination; only the currently assigned receiver may clear it.

### System

```text
STATUS
VERSION
UPTIME
BOOT
DEBUG ON
DEBUG OFF
```

- `STATUS` — CCA version, uptime, battery, PIR state/count, private alert destination, HOBO pointer.
- `VERSION` — firmware/version/schema/Meshtastic base/hardware target.
- `UPTIME` — uptime + persistent boot count.
- `BOOT` — persistent boot count + firmware identity.
- `DEBUG ON` — extra USB serial CCA diagnostics until reboot.
- `DEBUG OFF` — stop extra CCA serial diagnostics.

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

- `PIR` / `PIR STATUS` — current PIR state, counts, last event, TX state, private alert destination, pin.
- `PIR COUNT` — persistent total + since-boot count.
- `PIR LAST` — time since last event during this boot.
- `PIR RESET` — clear persistent PIR total and since-boot count.
- `PIR ON` / `PIR OFF` — enable/disable monitoring; persists.
- `PIR TX ON` / `PIR TX OFF` — enable/disable automatic PIR alert DMs; persists.

### Power / solar diagnostics

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

- `POWER` / `POWER STATUS` — voltage, battery %, charging indication, trend, min/max, samples.
- `POWER VOLTAGE` — current voltage/%/charging indication.
- `POWER MINMAX` — min/max/current since boot or last reset.
- `POWER TREND` — now, ~1 h, ~6 h, ~24 h and trend.
- `POWER HISTORY` — now, ~1 h, ~6 h, ~12 h, ~24 h, min/max.
- `POWER RESET` — clear RAM-only power history/min/max; does not reboot or clear PIR counts.
- `POWER UPTIME` — uptime + persistent boot count.

Power samples are stored about every 10 minutes. Current thresholds:

```text
LOW      < 3.60 V
CRITICAL < 3.45 V
RECOVERY   3.65 V hysteresis point
```

This is battery-trend diagnostics. It does not directly measure panel voltage, solar current, or charge wattage.

### HOBO

```text
LOGGER
READ
LOCK
UNLOCK
```

- `LOGGER` — logger model/MAC/BLE/logging interval/lock information.
- `READ` — immediate fresh HOBO reading without disturbing automatic schedule.
- `LOCK` — persist the currently identified HOBO BLE MAC.
- `UNLOCK` — clear logger assignment and resume discovery.

Supported families remain MX2001, MX2201, and MX2203.

---

## 7. Persistence

Survives reboot:

- PIR ON/OFF
- PIR TX ON/OFF
- PIR persistent total
- boot count
- **private automatic-alert destination**
- existing HOBO logger lock

Starts fresh after reboot:

- PIR since-boot count
- `PIR LAST` uptime reference
- power-history RAM buffer/min/max
- DEBUG state

---

## 8. Data path

```text
SEN0171 / HOBO
      ↓
CCA Seeed XIAO + Wio-SX1262
      ↓
Meshtastic mesh
      ↓
Heltec V4 gateway
      ↓
Neon
      ↓
Vercel
```

Existing HOBO packet formats are unchanged.

CCA automatic alerts are now **direct messages to the configured receiver**, so they are no longer ordinary public LongFast text broadcasts.

---

## 9. Windows local repo

```text
C:\Meshtastic\HOBO\firmware
```

Switch/update:

```powershell
cd C:\Meshtastic\HOBO\firmware
git fetch origin
git switch CCA-MX-HOBO-PIR-SEEED-v1
git pull --ff-only origin CCA-MX-HOBO-PIR-SEEED-v1
git branch --show-current
```

Must print:

```text
CCA-MX-HOBO-PIR-SEEED-v1
```

---

## 10. Build

```powershell
.\CCA-MX-HOBO-PIR\build.ps1
```

Manual:

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
& $pio run -e seeed_xiao_nrf52840_cca_mx_pir
```

---

## 11. Flash

```powershell
.\CCA-MX-HOBO-PIR\flash.ps1
```

Manual:

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
& $pio run -e seeed_xiao_nrf52840_cca_mx_pir -t upload
```

---

## 12. Serial monitor

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
& $pio device monitor -b 115200
```

Expected 1.0.1 startup includes:

```text
CCA-MX-PIR 1.0.1: D6 PIR, schema 1
CCA PIR: ON TX=ON ...
CCA automatic alerts: PRIVATE destination ...
```

Exit with `Ctrl+C`.

---

## 13. First test after flashing 1.0.1

From the radio you want to receive alerts:

```text
VERSION
ALERTS HERE
ALERTS STATUS
STATUS
LOGGER
PIR STATUS
POWER
```

Trigger the PIR after it has returned LOW.

Expected result: `PIR|ALERT|COUNT=...` appears in the **DM conversation**, not LongFast.

Then:

```text
PIR COUNT
PIR LAST
POWER HISTORY
```

Reboot the CCA node and verify:

```text
VERSION
ALERTS STATUS
BOOT
PIR COUNT
```

The private alert destination, persistent PIR total, and boot count should survive.

---

## 14. Immediate safety command for older 1.0.0 firmware

If a node is still running **CCA-MX-PIR 1.0.0**, its automatic CCA alerts were implemented as channel-0 broadcasts.

Until it is reflashed with 1.0.1, stop PIR broadcasts by DMing:

```text
PIR TX OFF
```

Then update, rebuild, and flash 1.0.1.

---

## 15. Rollback

Known-good HOBO-only branch:

```text
hobo-mx2001-mx2201-mx2203
```

```powershell
git fetch origin
git switch hobo-mx2001-mx2201-mx2203
git pull --ff-only origin hobo-mx2001-mx2201-mx2203
```
