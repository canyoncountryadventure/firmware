# CCA MX + HOBO + PIR — Seeed XIAO

> **USE THIS BRANCH:** `CCA-MX-HOBO-PIR-SEEED-v1`
>
> **Firmware:** `CCA-MX-PIR 1.0.7` · Schema `1` · Meshtastic `2.7.26`
>
> **Hardware:** Seeed XIAO nRF52840 + Wio-SX1262 + DFRobot SEN0171 PIR + optional HOBO MX2001/MX2201/MX2203
>
> **Build target:** `seeed_xiao_nrf52840_cca_mx_pir`
>
> **PIR signal:** **D6**
>
> **D0/A0:** reserved for the soil/rock build; soil moisture is **not** compiled into this branch.
>
> **AUTOMATIC CCA ALERTS ARE PRIVATE DMs ONLY. THERE IS NO PUBLIC LONGFAST FALLBACK.**

For the detailed technical reference, see [`START_HERE_CCA_MX_PIR.md`](START_HERE_CCA_MX_PIR.md).

---

## Production status

This branch contains the same RF-hardened CCA PIR core used by the combined soil/rock branch.

### PIR bug that was found during soil/rock testing

The SEN0171 signal on D6 can be driven HIGH by RF from this node's own Wio-SX1262 LoRa transmission. The PIR can then remain HIGH for several seconds. With ordinary LOW→HIGH edge code, that creates a dangerous feedback path:

```text
LoRa TX
  ↓
RF induces false D6 HIGH
  ↓
firmware thinks motion occurred
  ↓
sends PIR alert
  ↓
more LoRa TX / more false PIR activity
```

A normal debounce is not enough because the RF-induced HIGH can outlast the debounce interval.

### Final RF hardening in 1.0.7

- D6 is configured with `INPUT_PULLDOWN` instead of plain `INPUT`.
- The firmware watches the Meshtastic RadioLib TX state.
- A new D6 HIGH that starts during local LoRa TX or within **15 seconds after local TX** is rejected.
- Once an RF-correlated HIGH is rejected, that entire HIGH pulse stays rejected until D6 physically returns LOW.
- Only a fresh LOW→HIGH transition after physical LOW can become a valid PIR event.
- A legitimate PIR HIGH that began before a later LoRa TX remains valid.
- The combined soil/rock branch uses this same filtered D6 signal for its rock packet motion field/count; raw D6 is not used there.

This is the fix discovered and bench-validated during the soil-moisture/rock work. Do not remove or shorten the 15-second RF guard without new bench evidence.

### Other validated behavior retained

- D6 PIR initialization and real motion detection
- private automatic PIR alerts
- remote CCA DM commands
- PKI-encrypted direct messages
- universal HOBO MX2001/MX2201/MX2203 support
- `LOGGER`, `READ`, `LOCK`, `UNLOCK`
- power-status/history commands
- persistent PIR total / boot count / alert destination

**Radio note:** the Seeed XIAO + Wio-SX1262 build actually applies **22 dBm maximum TX power**, even if a stored Meshtastic setting displays 30 dBm.

---

# 1. Wiring

| SEN0171 | XIAO |
|---|---|
| VCC | 3V3 |
| GND | GND |
| Digital signal | **D6** |

If the bench PIR already has a 100 kΩ external pull-down from signal to GND, it may remain installed. Firmware 1.0.7 also enables the nRF52840 internal pulldown on D6.

### Pin plan

| XIAO pin | Role |
|---|---|
| **D0 / A0** | Reserved for soil/rock build; unused here |
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

GNSS is compiled out of this dedicated target so D6 is not taken by the normal Seeed GNSS UART mapping.

---

# 2. PIR behavior

The SEN0171 is used as a **presence / tamper / security sensor**, not as a high-speed individual trail counter.

Normal logic:

1. Firmware waits for D6 LOW before arming.
2. A valid fresh LOW→HIGH edge increments the persistent PIR total and since-boot count.
3. If `PIR TX ON` and a private alert destination is configured, an immediate private DM is queued.
4. While the PIR remains HIGH, no duplicate event is counted.
5. The PIR re-arms after D6 returns LOW.

RF hardening sits in front of that logic. An RF-correlated HIGH is presented to the motion logic as LOW for the entire physical HIGH pulse.

Automatic PIR message:

```text
PIR|ALERT|COUNT=17
```

---

# 3. Private automatic alerts

Automatic CCA alerts never broadcast to public LongFast.

From the Meshtastic radio that should receive alerts, **DM the field node**:

```text
ALERTS HERE
```

Check it:

```text
ALERTS STATUS
```

Expected form:

```text
Alerts: PRIVATE DM ONLY
Destination: !xxxxxxxx
Public fallback: DISABLED
```

The destination is stored persistently.

To clear it, send from the currently assigned receiver:

```text
ALERTS CLEAR
```

If no destination is configured, automatic CCA alerts are suppressed rather than broadcast.

Automatic private CCA messages include:

```text
PIR|ALERT|COUNT=17
SYS|BOOT|COUNT=7|FW=CCA-MX-PIR-1.0.7
POWER|LOW|V=3.590
POWER|CRITICAL|V=3.440
POWER|RECOVERED|V=3.670
```

---

# 4. Complete DM command reference

Send these as a **Direct Message to the CCA node**. Public/broadcast channel messages are ignored by the CCA command handler. Commands are case-insensitive and a leading `/` is optional.

## Alert routing

| Command | Function |
|---|---|
| `ALERTS HERE` | Save the sending radio as the persistent private alert destination |
| `ALERTS STATUS` | Show the destination and confirm public fallback is disabled |
| `ALERTS CLEAR` | Clear the destination; only the current destination may clear it |

## System

| Command | Function |
|---|---|
| `STATUS` | Overall CCA status, battery, PIR state/count, alert destination |
| `VERSION` | Firmware identity, schema, Meshtastic base, hardware target |
| `UPTIME` | Uptime + persistent boot count |
| `BOOT` | Persistent boot count + firmware identity |
| `DEBUG ON` | Extra CCA diagnostics on USB serial until reboot |
| `DEBUG OFF` | Disable extra CCA serial diagnostics |

`DEBUG` changes serial logging only. It does not change PIR sensitivity, LoRa power, HOBO timing, or mesh reporting.

## PIR

| Command | Function |
|---|---|
| `PIR` | Full PIR status |
| `PIR STATUS` | Same full PIR status |
| `PIR COUNT` | Persistent total + since-boot detections |
| `PIR LAST` | Time since last valid PIR event this boot |
| `PIR RESET` | Clear persistent PIR total, since-boot count, and last-event reference |
| `PIR ON` | Enable PIR monitoring; persists |
| `PIR OFF` | Disable PIR monitoring; persists |
| `PIR TX ON` | Enable private automatic PIR DMs; persists |
| `PIR TX OFF` | Continue local counting but suppress PIR DMs; persists |

## Power

| Command | Function |
|---|---|
| `POWER` | Voltage, %, charging indication, trend, min/max, sample count |
| `POWER STATUS` | Same summary as `POWER` |
| `POWER VOLTAGE` | Current voltage, battery %, charging indication |
| `POWER MINMAX` | Min, max, current voltage since boot / reset |
| `POWER TREND` | Now, ~1 h, ~6 h, ~24 h references and trend |
| `POWER HISTORY` | Now, ~1 h, ~6 h, ~12 h, ~24 h plus min/max |
| `POWER RESET` | Clear RAM-only power history/min/max; does not reboot or erase PIR counts |
| `POWER UPTIME` | Uptime + persistent boot count |

Power history samples approximately every 10 minutes and is RAM-only. Low threshold is 3.60 V, critical is 3.45 V, and recovery hysteresis is 3.65 V.

## HOBO

| Command | Function |
|---|---|
| `LOGGER` | Logger model/MAC/BLE/log interval/lock information |
| `READ` | Immediate fresh HOBO reading without changing automatic schedule |
| `LOCK` | Persist the currently identified HOBO BLE MAC |
| `UNLOCK` | Clear logger assignment and resume supported-HOBO discovery |

Supported logger families remain **MX2001, MX2201, and MX2203**.

---

# 5. Windows — update local repo

Local repo used for this build:

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

The last command must print:

```text
CCA-MX-HOBO-PIR-SEEED-v1
```

---

# 6. Build, flash, monitor

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

Expected identity:

```text
CCA-MX-PIR 1.0.7
```

Exit serial monitor with `Ctrl+C`.

---

# 7. Required bench check after flashing 1.0.7

From the intended receiving radio, DM:

```text
VERSION
ALERTS HERE
ALERTS STATUS
STATUS
LOGGER
PIR STATUS
POWER
```

Then test both cases:

### Real motion

Let the PIR return LOW, move in front of it once, and verify exactly one valid PIR count/private alert.

### Self-TX rejection

Send several commands that force the node to reply over LoRa while nobody is moving. Continue watching `PIR COUNT` for at least the full PIR hold/guard period. The count must **not** increase from the node's own transmissions.

After reboot, verify:

```text
VERSION
ALERTS STATUS
PIR COUNT
BOOT
```

The alert destination, PIR total, and boot counter should persist. Power-history RAM intentionally starts over.

---

# 8. Related branches

### Combined soil/rock production branch

```text
CCA-MX-HOBO-PIR-ROCK-SEEED-v1
```

That branch adds SEN0308 sandstone/soil telemetry on D0/A0 and uses the same RF-filtered D6 PIR core.

### Known-good HOBO reference

```text
hobo-mx2001-mx2201-mx2203
```

Keep this branch as the clean HOBO-only rollback/reference.

### Legacy trail experiment

```text
trail-sen0171
```

This is historical test firmware. Do **not** treat its old `person walked by...` behavior as production PIR logic.
