# CCA MX + HOBO + PIR — Seeed XIAO

> **AUTHORITATIVE FIELD BRANCH:** `CCA-MX-HOBO-PIR-SEEED-v1`
>
> **Firmware:** `CCA-MX-PIR 1.0.7`
>
> **Meshtastic base:** `2.7.26`
>
> **Build target:** `seeed_xiao_nrf52840_cca_mx_pir`
>
> **Hardware:** Seeed XIAO nRF52840 + Wio-SX1262 + DFRobot SEN0171 PIR + optional HOBO MX2001/MX2201/MX2203
>
> **PIR:** D6
>
> **SOIL:** not compiled into this branch; D0/A0 is reserved for the combined rock/soil branch.
>
> **AUTOMATIC CCA ALERTS ARE PRIVATE DMs ONLY. THERE IS NO PUBLIC LONGFAST FALLBACK.**

This README is the front-page operating reference for the production Seeed HOBO + PIR firmware. For deeper implementation history, see [`START_HERE_CCA_MX_PIR.md`](START_HERE_CCA_MX_PIR.md).

---

## DO NOT CHANGE THESE WITHOUT A NEW BENCH VALIDATION

The current production baseline is deliberately frozen around the behavior that was tested on the actual XIAO + Wio-SX1262 + SEN0171 hardware.

```text
Firmware identity:        CCA-MX-PIR 1.0.7
Meshtastic base:          2.7.26
PIR pin:                  D6
PIR poll interval:        100 ms
PIR input mode:           INPUT_PULLDOWN
Post-local-TX RF guard:   15,000 ms
PIR re-arm requirement:   physical LOW
Valid PIR event:          fresh LOW -> HIGH only
Seeed radio TX limit:     22 dBm
```

Do not shorten the RF guard, remove the D6 pull-down, re-enable raw D6 event handling, or move the PIR onto a Wio radio pin without re-running the RF self-trigger test described below.

---

# 1. What this firmware does

```text
Meshtastic 2.7.26
├── normal Meshtastic LoRa mesh radio
├── normal Meshtastic BLE connection to phone/computer
├── HOBO MX2001 BLE reader
├── HOBO MX2201 BLE reader
├── HOBO MX2203 BLE reader
├── automatic HOBO telemetry over the mesh
├── SEN0171 PIR presence/tamper sensor on D6
├── RF self-trigger rejection for PIR
├── private automatic PIR alerts
├── battery/power diagnostics and history
├── persistent boot / PIR / destination state
└── remote DM commands
```

The node still functions as a normal Meshtastic radio while the CCA code runs beside it.

The SEN0171 is used as a **presence / tamper / security sensor**, not as a high-speed individual trail counter. Its physical HIGH hold can last several seconds, so closely spaced people can merge into one event.

---

# 2. Critical PIR RF fix

Bench testing found that LoRa transmissions from the node itself could induce a false HIGH on the SEN0171/D6 circuit.

Without protection, the failure path can be:

```text
LoRa TX
  ↓
RF induces D6 HIGH
  ↓
firmware thinks motion occurred
  ↓
PIR alert causes another LoRa TX
  ↓
more false PIR activity
```

A normal debounce is not enough because the induced HIGH can persist for seconds.

## Production 1.0.7 behavior

- D6 uses the nRF52840 internal `INPUT_PULLDOWN`.
- Firmware watches the local RadioLib transmit state.
- A **new** D6 HIGH beginning during local LoRa TX or within the **15-second post-TX guard** is rejected as RF-correlated.
- Once rejected, that entire physical HIGH pulse stays rejected.
- The filter clears only after D6 physically returns LOW.
- A new valid event requires a fresh LOW→HIGH transition after that LOW.
- Legitimate motion that began before a later LoRa transmission is not invalidated.

This behavior is part of the production definition of **CCA-MX-PIR 1.0.7**.

---

# 3. Wiring

## SEN0171 PIR

| SEN0171 | Seeed XIAO |
|---|---|
| VCC | 3V3 |
| GND | GND |
| Digital signal | **D6** |

If the bench PIR already has a 100 kΩ external pull-down from signal to GND, it may remain installed. Firmware also enables the nRF52840 internal pull-down on D6.

## XIAO / Wio-SX1262 pin plan

| XIAO pin | Function |
|---|---|
| **D0 / A0** | Reserved for combined soil/rock branch; unused here |
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

GNSS is compiled out of this dedicated target so the normal Seeed GNSS D6 UART mapping cannot take the PIR pin.

---

# 4. Fastest Windows build + flash from nothing

No permanent source checkout is required. The source can live entirely under `%TEMP%` and be deleted after flashing.

Paste into a VS Code PowerShell terminal:

```powershell
$work = "$env:TEMP\cca-hobo-pir"
Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue

git clone --recurse-submodules --branch CCA-MX-HOBO-PIR-SEEED-v1 https://github.com/canyoncountryadventure/firmware.git $work
cd $work

.\CCA-MX-HOBO-PIR\build.ps1
.\CCA-MX-HOBO-PIR\flash.ps1
```

Expected firmware identity after flashing:

```text
CCA-MX-PIR 1.0.7
```

After the radio is verified, the temporary source can be removed:

```powershell
cd $env:TEMP
Remove-Item "$env:TEMP\cca-hobo-pir" -Recurse -Force
```

The firmware remains on the XIAO after the temporary source folder is deleted.

---

# 5. Build only / flash only

If the temporary checkout already exists:

```powershell
cd "$env:TEMP\cca-hobo-pir"
.\CCA-MX-HOBO-PIR\build.ps1
```

Then:

```powershell
.\CCA-MX-HOBO-PIR\flash.ps1
```

The scripts use the production PlatformIO environment:

```text
seeed_xiao_nrf52840_cca_mx_pir
```

---

# 6. Serial monitor

Find the current COM port:

```powershell
Get-CimInstance Win32_SerialPort | Select-Object DeviceID,Name
```

Then, for example on COM8:

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
& $pio device monitor --port COM8 -b 115200
```

Exit with `Ctrl+C`.

The bootloader COM port and the running-firmware COM port can differ after a flash. If the port disappears or changes, run the COM-port command again.

---

# 7. XIAO recovery if BLE and serial disappear

If the LED/power rail is alive but Meshtastic BLE disappears and normal serial will not open:

1. Keep the XIAO connected by USB.
2. Rapidly **double-tap RESET** on the XIAO.
3. Check Windows again:

```powershell
Get-Volume | Where-Object FileSystemLabel -like "XIAO*"
Get-CimInstance Win32_SerialPort | Select-Object DeviceID,Name
```

If the XIAO bootloader drive appears, the MCU is alive and can be reflashed.

After a successful build, the generated UF2 is under:

```text
.pio\build\seeed_xiao_nrf52840_cca_mx_pir\
```

If normal serial upload is troublesome, the UF2 can be copied to the XIAO bootloader drive manually.

---

# 8. PIR event behavior

At boot:

- If D6 is LOW, PIR can arm normally.
- If D6 is HIGH, firmware waits for a physical LOW before arming.
- A boot-time HIGH is not automatically counted as motion.

For a valid event:

1. Filtered D6 transitions LOW→HIGH.
2. Persistent PIR total increments.
3. Since-boot count increments.
4. Last valid event time updates.
5. Persistent state is saved.
6. If `PIR TX ON` and an alert destination exists, a private DM is sent.

Example:

```text
PIR|ALERT|COUNT=17
```

While the physical PIR remains HIGH, the event does not repeat. The next valid event requires physical LOW followed by a new HIGH.

---

# 9. Private automatic alerts

Automatic CCA alerts **never** fall back to a public LongFast broadcast.

From the radio that should receive alerts, open a **Direct Message to the CCA field node** and send:

```text
ALERTS HERE
```

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

To clear the assigned receiver, send from that receiver:

```text
ALERTS CLEAR
```

If no private destination is configured, automatic CCA alerts are suppressed rather than sent publicly.

Automatic private messages include:

```text
PIR|ALERT|COUNT=17
SYS|BOOT|COUNT=7|FW=CCA-MX-PIR-1.0.7
POWER|LOW|V=3.590
POWER|CRITICAL|V=3.440
POWER|RECOVERED|V=3.670
```

---

# 10. Complete DM command reference

CCA commands must be sent as a **DM to the CCA node**. The handler ignores ordinary broadcast-channel text. Commands are case-insensitive and may optionally begin with `/`.

## Alert routing

| Command | Function |
|---|---|
| `ALERTS HERE` | Save the sending radio as the persistent private alert destination |
| `ALERTS STATUS` | Show destination and private-only status |
| `ALERTS CLEAR` | Clear the destination |

## System

| Command | Function |
|---|---|
| `STATUS` | Overall CCA status, battery, PIR state/count, alert destination |
| `VERSION` | Firmware identity, schema, Meshtastic base, hardware target |
| `UPTIME` | Uptime + persistent boot count |
| `BOOT` | Persistent boot count + firmware identity |
| `DEBUG ON` | Extra CCA USB serial diagnostics until reboot |
| `DEBUG OFF` | Disable extra CCA serial diagnostics |

## PIR

| Command | Function |
|---|---|
| `PIR` | Full PIR status |
| `PIR STATUS` | Full PIR status |
| `PIR COUNT` | Persistent total + since-boot count |
| `PIR LAST` | Time since last valid event this boot |
| `PIR RESET` | Clear PIR total / since-boot count / last-event reference |
| `PIR ON` | Enable PIR monitoring; persists |
| `PIR OFF` | Disable PIR monitoring; persists |
| `PIR TX ON` | Enable automatic private PIR DMs; persists |
| `PIR TX OFF` | Keep counting locally but suppress PIR DMs; persists |

## Power

| Command | Function |
|---|---|
| `POWER` | Voltage, %, charging indication, trend, min/max, sample count |
| `POWER STATUS` | Same summary as `POWER` |
| `POWER VOLTAGE` | Current voltage, battery %, charging indication |
| `POWER MINMAX` | Min, max, current voltage since boot/reset |
| `POWER TREND` | Current and approximate historical references |
| `POWER HISTORY` | Current, ~1 h, ~6 h, ~12 h, ~24 h plus min/max |
| `POWER RESET` | Clear RAM-only power history/min/max |
| `POWER UPTIME` | Uptime + persistent boot count |

Power history samples approximately every 10 minutes and is RAM-only.

```text
LOW:       below 3.60 V
CRITICAL:  below 3.45 V
RECOVERY:  3.65 V hysteresis point
```

## HOBO

| Command | Function |
|---|---|
| `LOGGER` | Logger model/MAC/BLE/log interval/lock information |
| `READ` | Request a fresh HOBO reading without changing the automatic schedule |
| `LOCK` | Persist the currently identified HOBO BLE MAC |
| `UNLOCK` | Clear logger assignment and resume discovery |

Supported logger families:

```text
MX2001
MX2201
MX2203
```

---

# 11. Persistence

## Survives reboot

- PIR ON/OFF
- PIR TX ON/OFF
- persistent PIR total
- boot count
- private alert destination
- HOBO lock/assignment through the HOBO module

## Resets at reboot

- PIR count since this boot
- `PIR LAST` uptime reference
- power-history ring buffer
- power min/max
- DEBUG state

`PIR RESET` intentionally clears the persistent PIR total.

`POWER RESET` affects only RAM power-history/min/max state. It does not reboot the node or erase PIR, alert, HOBO, or Meshtastic configuration.

---

# 12. Required post-flash bench test

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

## Test A — real motion

1. Let D6/PIR return LOW.
2. Walk in front of the PIR once.
3. Verify exactly one valid count.
4. Verify one private alert if `PIR TX ON`.

## Test B — RF self-trigger rejection

1. Nobody moves in front of the PIR.
2. Repeatedly DM commands that make the node transmit replies:

```text
STATUS
POWER
PIR STATUS
LOGGER
```

3. Continue through the full 15-second guard and physical PIR HIGH period if one occurs.
4. Check:

```text
PIR COUNT
```

The count must **not** increase because of the node's own LoRa transmissions.

## Test C — recovery after RF event

After any RF-correlated raw HIGH physically returns LOW, perform real motion again. A fresh real LOW→HIGH must work normally.

## Test D — reboot persistence

Power-cycle and check:

```text
VERSION
ALERTS STATUS
PIR COUNT
BOOT
LOGGER
```

---

# 13. Related branches

## Combined soil / rock build

```text
CCA-MX-HOBO-PIR-ROCK-SEEED-v1
```

Adds SEN0308 sandstone/soil telemetry on D0/A0 and uses the same RF-filtered D6 PIR concept.

## HOBO-only rollback/reference

```text
hobo-mx2001-mx2201-mx2203
```

Use as the clean HOBO-only reference if PIR functionality needs to be isolated.

## Legacy trail experiment

```text
trail-sen0171
```

Historical test firmware only. Do not use its old trail-count behavior as the production PIR implementation.

---

# 14. Production identity checklist

Before deploying a field node, confirm all of these:

```text
Branch:       CCA-MX-HOBO-PIR-SEEED-v1
Firmware:     CCA-MX-PIR 1.0.7
Meshtastic:   2.7.26
Board:        Seeed XIAO nRF52840 + Wio-SX1262
PIR:          SEN0171 on D6
HOBO:         MX2001 / MX2201 / MX2203
RF guard:     15 seconds after local TX
Alert mode:   private DM only
Radio power:  22 dBm max on this hardware
```

If those do not match, stop and verify the build before field deployment.
