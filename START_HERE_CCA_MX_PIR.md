# START HERE — CCA MX + HOBO + PIR (Seeed)

> **AUTHORITATIVE CCA v1 BRANCH:** `CCA-MX-HOBO-PIR-SEEED-v1`
>
> **Build target:** `seeed_xiao_nrf52840_cca_mx_pir`
>
> **Firmware identity:** `CCA-MX-PIR 1.0.0`
>
> **Schema:** `1`
>
> **Meshtastic base:** `2.7.26`
>
> **PIR signal pin:** **D6**
>
> **D0/A0:** reserved for future SEN0308 soil moisture; soil is **not implemented in v1**

If you are just trying to use, flash, or text the node, the root [`README.md`](README.md) is the fastest command/reference sheet. This file explains the design and field behavior in more detail.

---

## 1. Current validation status

### Build validation

GitHub Actions successfully compiled and linked the exact target:

```text
seeed_xiao_nrf52840_cca_mx_pir
```

The build included both:

- existing universal HOBO MX2001/MX2201/MX2203 support;
- new `CCAStationModule` for PIR, power history, boot health, and remote CCA commands.

### Physical bench validation

Observed on the actual Seeed XIAO nRF52840 + Wio-SX1262 node:

- CCA module startup: **PASS**
- Wio-SX1262 initialization: **PASS**
- D6 PIR initialization: **PASS**
- PIR LOW→HIGH detection: **PASS**
- persistent-total counter increments during operation: **PASS**
- immediate `PIR|ALERT|COUNT=...` LoRa transmission: **PASS**
- mesh rebroadcast of PIR alert: **PASS**
- remote PKI-encrypted DMs: **PASS**
- `STATUS`, `POWER`, `POWER TREND`, `PIR STATUS`: **PASS**
- existing `LOGGER` / `UNLOCK` HOBO command handling: **PASS**
- universal HOBO module remains active/scanning while PIR operates: **PASS**
- deliberate power-cycle persistence test: **still to verify**

Bench logs showed:

```text
CCA-MX-PIR 1.0.0: D6 PIR, schema 1
CCA PIR: ON TX=ON startup=ARMED
CCA PIR ALERT total=1 boot=1
CCA PIR ALERT total=2 boot=2
```

The HOBO module also initialized independently and continued scanning/responding during the same session.

### TX power note

The saved Meshtastic setting can show 30 dBm, but on this Seeed/Wio-SX1262 target the firmware reports and applies:

```text
Final Tx power: 22 dBm
Power output set to 22
```

Treat **22 dBm** as the actual maximum TX power for this node.

---

## 2. Hardware and pin map

Hardware for CCA v1:

- Seeed Studio XIAO nRF52840
- Wio-SX1262 for XIAO, standalone/kit mapping
- DFRobot SEN0171 PIR motion sensor
- battery / solar charger supplying the node
- optional HOBO MX2001, MX2201, or MX2203 over BLE

### SEN0171 wiring

| SEN0171 | XIAO |
|---|---|
| VCC | 3V3 |
| GND | GND |
| Digital output | **D6** |

If the bench PIR already has a 100 kΩ pull-down from signal to GND, retain it with the D6 signal wiring.

### Complete exposed-pin plan

| XIAO pin | CCA v1 role |
|---|---|
| **D0 / A0** | **Reserved for future SEN0308 soil sensor — unused now** |
| D1 | SX1262 DIO1 |
| D2 | SX1262 RESET |
| D3 | SX1262 BUSY |
| D4 | SX1262 CS |
| D5 | SX1262 RX enable |
| **D6** | **SEN0171 PIR digital input** |
| D7 | Free in CCA v1 |
| D8 | SX1262 SPI SCK |
| D9 | SX1262 SPI MISO |
| D10 | SX1262 SPI MOSI |

### GNSS conflict prevention

The normal Seeed kit can use D6 for the L76K GNSS UART mapping. The dedicated CCA target compiles the Meshtastic GPS module out so D6 is owned by the PIR and cannot be reclaimed by GNSS behavior.

This only affects the custom CCA target. It does not modify the normal Seeed target or the known-good HOBO branch.

---

## 3. What the PIR actually does

The SEN0171 is being used here as a **remote presence/tamper alarm**, not as a high-speed trail counter.

Defaults:

- monitoring: ON
- transmissions: ON
- poll interval: 100 ms
- input: D6
- event: LOW→HIGH edge only
- no added software cooldown
- re-arms as soon as the physical SEN0171 output returns LOW

A sensor output that is already HIGH at startup is ignored until it returns LOW once. This prevents a false boot-time PIR alarm.

On a valid new event:

1. persistent total increments;
2. since-boot count increments;
3. last-detection uptime is recorded;
4. persistent state is written;
5. if PIR TX is enabled, a mesh text alert is transmitted immediately.

Example:

```text
PIR|ALERT|COUNT=17
```

The SEN0171 itself may remain HIGH for roughly 6–11 seconds. The firmware does not extend that hold period.

---

## 4. Persistent vs non-persistent state

### Survives reboot

- PIR enabled/disabled
- PIR TX enabled/disabled
- total PIR detection count
- boot count

### Resets at reboot

- detections since boot
- `PIR LAST` timing reference
- power-history ring buffer
- power-history min/max
- DEBUG state

`PIR RESET` intentionally clears the persistent PIR total.

`POWER RESET` does **not** affect PIR, boot count, Meshtastic configuration, or HOBO state. It only clears the in-RAM power-history statistics and starts them again from the current voltage.

---

## 5. How DM commands work

CCA commands are designed for remote field control from another Meshtastic radio.

### Rules

- Send commands as a **Direct Message to this node**.
- CCA commands are ignored when posted as ordinary public/broadcast channel messages.
- CCA matching is case-insensitive.
- A leading `/` is optional.
- Extra leading/trailing whitespace is ignored.

Therefore all of these are equivalent:

```text
POWER
Power
power
/POWER
```

The inherited HOBO command interface is also documented as case-insensitive with an optional leading slash.

---

## 6. COMPLETE DM COMMAND LIST

### System / overall node

#### `STATUS`
Returns a compact overall health summary including:

- CCA firmware/version
- uptime
- battery voltage / percentage
- charging indication reported by Meshtastic power status
- PIR ON/OFF
- PIR TX ON/OFF
- PIR total and since-boot count
- reminder to use `LOGGER` for detailed HOBO status

#### `VERSION`
Returns:

```text
FW: CCA-MX-PIR 1.0.0
Schema: 1
Meshtastic: 2.7.26
Build: SEEED XIAO + Wio-SX1262
```

#### `UPTIME`
Returns node uptime and persistent boot count.

#### `POWER UPTIME`
Same uptime/boot-count information.

#### `BOOT`
Returns persistent boot count plus CCA firmware identity.

Useful for detecting unexplained resets or someone disconnecting/reconnecting site power.

#### `DEBUG ON`
Enables extra **USB serial** CCA diagnostic lines until reboot.

It does **not**:

- change PIR sensitivity;
- change LoRa settings;
- increase mesh reporting;
- alter HOBO timing.

#### `DEBUG OFF`
Stops the extra CCA serial diagnostics.

---

### PIR / SEN0171

#### `PIR`
Full PIR status.

#### `PIR STATUS`
Same as `PIR`.

Typical fields:

```text
PIR: ON
Sensor: CLEAR
Total Detections: 17
Since Boot: 3
Last Detection: 2h 14m
TX Alerts: ON
Pin: D6
```

#### `PIR COUNT`
Returns:

- persistent total detections;
- detections during this boot.

#### `PIR LAST`
Returns time since the most recent detection during this boot.

After reboot, this reports no detection until a new PIR event occurs.

#### `PIR RESET`
Clears:

- persistent total PIR count;
- current-boot PIR count;
- last-detection reference.

#### `PIR ON`
Enables PIR monitoring and saves the setting to persistent state.

#### `PIR OFF`
Disables PIR monitoring and saves the setting.

#### `PIR TX ON`
Every newly detected LOW→HIGH PIR event transmits immediately. Persists across reboot.

#### `PIR TX OFF`
The node continues sensing/counting locally but PIR alerts are silent over the mesh. Persists across reboot.

---

### Power / solar performance diagnostics

The CCA module uses the same battery voltage / battery-status information Meshtastic already reads from the XIAO hardware.

It does **not** directly measure:

- solar-panel voltage;
- panel current;
- charger current;
- generated wattage.

The goal is to infer whether the system is maintaining charge by following battery behavior over time.

A power sample is stored approximately every **10 minutes**, with room for about 24 hours of history.

#### `POWER`
Compact power summary:

- voltage
- Meshtastic battery-percent estimate
- charging indication
- 6-hour trend once enough samples exist
- min/max since boot or `POWER RESET`
- sample count

#### `POWER STATUS`
Same summary as `POWER`.

#### `POWER VOLTAGE`
Returns current:

- battery voltage
- battery percentage estimate
- charging indication

#### `POWER MINMAX`
Returns min, max, and current voltage since boot / last `POWER RESET`.

#### `POWER TREND`
Returns references near:

- now
- 1 hour ago
- 6 hours ago
- 24 hours ago

and classifies the 6-hour direction as rising, falling, stable, or learning.

#### `POWER HISTORY`
Returns:

- now
- ~1 h
- ~6 h
- ~12 h
- ~24 h
- min/max

Young nodes show missing historical references until enough samples have actually accumulated.

#### `POWER RESET`
Clears only the in-RAM power-history ring buffer and power min/max, then records the current voltage as the new starting point.

It does **not** reboot the node.

It does **not** reset PIR counts.

It does **not** reset boot count.

It does **not** erase Meshtastic configuration.

### Automatic power thresholds

Current v1 thresholds:

- below 3.60 V → LOW
- below 3.45 V → CRITICAL
- recovery hysteresis point → 3.65 V

Automatic packets:

```text
POWER|LOW|V=3.590
POWER|CRITICAL|V=3.440
POWER|RECOVERED|V=3.670
```

Alerts occur on state changes rather than every 10-minute sample.

**Bench note:** instantaneous battery readings can move noticeably around radio activity. Treat trend/history as diagnostic information rather than precision solar instrumentation.

---

### HOBO MX2001 / MX2201 / MX2203

The existing universal HOBO implementation is intentionally preserved.

#### `LOGGER`
Shows the current/identified logger information, including available model/MAC/BLE/logging interval/lock details.

#### `READ`
Requests an immediate fresh logger read without changing the normal automatic record-aligned reporting schedule.

#### `LOCK`
Persists the currently identified HOBO BLE MAC so the radio reconnects only to that logger after reboot.

Field workflow:

1. leave unlocked while bench testing;
2. deploy beside the intended logger;
3. send `LOGGER` and verify it;
4. send `LOCK`.

#### `UNLOCK`
Clears the saved logger assignment and resumes supported-HOBO discovery.

Automatic HOBO reporting remains:

- MX2001: existing compact private-app binary packet for water level + temperature;
- MX2201/MX2203: existing Meshtastic environmental telemetry packet;
- cadence tied to confirmed new HOBO records rather than an unrelated free-running timer.

A HOBO BLE failure does not stop the CCA PIR thread.

---

## 7. Automatic CCA mesh messages

These do not require a DM command.

### PIR event

```text
PIR|ALERT|COUNT=17
```

### Boot event

Approximately 30 seconds after boot:

```text
SYS|BOOT|COUNT=7|FW=CCA-MX-PIR-1.0.0
```

### Power state events

```text
POWER|LOW|V=3.590
POWER|CRITICAL|V=3.440
POWER|RECOVERED|V=3.670
```

These are intentionally compact and machine-readable so the Heltec V4 gateway can later normalize them into Neon.

---

## 8. Data architecture

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

Existing HOBO packet formats are unchanged by this CCA branch.

The new PIR/power/system events arrive at the Heltec as normal Meshtastic text packets. Dedicated Neon fields/tables require the Heltec ingestion/parser to explicitly recognize those new prefixes; that backend work is separate from this sensor-node build.

---

## 9. Windows local repo

Current local repo location used during the physical build/test:

```text
C:\Meshtastic\HOBO\firmware
```

Switch to the exact CCA branch:

```powershell
cd C:\Meshtastic\HOBO\firmware
git fetch origin
git switch CCA-MX-HOBO-PIR-SEEED-v1
git pull --ff-only origin CCA-MX-HOBO-PIR-SEEED-v1
git branch --show-current
```

The last line must print:

```text
CCA-MX-HOBO-PIR-SEEED-v1
```

---

## 10. Build

```powershell
.\CCA-MX-HOBO-PIR\build.ps1
```

Manual equivalent:

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
& $pio run -e seeed_xiao_nrf52840_cca_mx_pir
```

---

## 11. Flash

```powershell
.\CCA-MX-HOBO-PIR\flash.ps1
```

Manual equivalent:

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
& $pio run -e seeed_xiao_nrf52840_cca_mx_pir -t upload
```

Do not use the ordinary `seeed_xiao_nrf52840_kit` target when you intend to test the CCA PIR firmware.

---

## 12. Serial monitor

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
& $pio device monitor -b 115200
```

Exit with:

```text
Ctrl+C
```

If the port opens but boot lines do not appear, press the XIAO reset button once.

---

## 13. Recommended bench sequence

### Basic operation

DM:

```text
VERSION
STATUS
LOGGER
PIR STATUS
POWER
```

Trigger the PIR once after it has returned LOW.

Expect:

```text
PIR|ALERT|COUNT=<new total>
```

Then DM:

```text
PIR COUNT
PIR LAST
POWER HISTORY
```

### Persistence test

1. DM `PIR COUNT` and write down the total.
2. DM `BOOT` and write down boot count.
3. Power the node fully off.
4. Power it back on.
5. Wait for startup.
6. DM `PIR COUNT` — persistent total should remain.
7. DM `BOOT` — boot count should increase.
8. DM `POWER HISTORY` — history should have restarted after boot.

### PIR TX-off test

1. DM `PIR TX OFF`.
2. Trigger one new PIR event.
3. DM `PIR COUNT` — count should increase.
4. Confirm no automatic PIR alert was sent.
5. DM `PIR TX ON` to restore normal field behavior.

### HOBO coexistence test

1. DM `LOGGER`.
2. DM `READ`.
3. Trigger PIR while BLE/HOBO work is active.
4. Confirm the PIR still detects/transmits.
5. If logger is intentionally absent, confirm PIR and power commands remain responsive.

---

## 14. Rollback / known-good reference

The known-good universal HOBO branch is intentionally left untouched:

```text
hobo-mx2001-mx2201-mx2203
```

Rollback:

```powershell
git fetch origin
git switch hobo-mx2001-mx2201-mx2203
git pull --ff-only origin hobo-mx2001-mx2201-mx2203
```

---

## 15. CCA-specific files

```text
README.md
START_HERE_CCA_MX_PIR.md
CCA-MX-HOBO-PIR/build.ps1
CCA-MX-HOBO-PIR/flash.ps1
src/modules/CCAStationModule.h
src/modules/CCAStationModule.cpp
.github/workflows/cca_mx_pir_build.yml
```

Integration points into the Meshtastic tree:

```text
src/modules/Modules.cpp
variants/nrf52840/seeed_xiao_nrf52840_kit/platformio.ini
```

The large universal HOBO implementation itself is not rewritten by the PIR addition. That separation is intentional so the proven logger code remains isolated from the new CCA field-station functions.
