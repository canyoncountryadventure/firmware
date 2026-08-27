# START HERE — CCA MX + HOBO + PIR (Seeed)

> **AUTHORITATIVE BRANCH:** `CCA-MX-HOBO-PIR-SEEED-v1`
>
> **Firmware:** `CCA-MX-PIR 1.0.7`
>
> **Build target:** `seeed_xiao_nrf52840_cca_mx_pir`
>
> **Meshtastic base:** `2.7.26`
>
> **PIR:** SEN0171 on **D6**
>
> **SOIL:** not compiled here; D0/A0 is reserved for the combined rock branch.

The root [`README.md`](README.md) is the complete command/field cheat sheet. This file records the firmware design, the fatal PIR bug found during soil/rock testing, and the requirements that must not regress.

---

## 1. Branch purpose

```text
Meshtastic 2.7.26
├── universal HOBO MX2001 / MX2201 / MX2203 BLE support
├── SEN0171 PIR presence/tamper sensor on D6
├── RF self-trigger rejection for D6
├── private automatic CCA alerts
├── power history / thresholds
├── boot / uptime diagnostics
└── remote DM commands
```

The SEN0171 is a **presence/tamper/security sensor** here. Its multi-second HIGH hold makes it unsuitable for counting individual closely-spaced trail users.

The combined sandstone/soil build lives on:

```text
CCA-MX-HOBO-PIR-ROCK-SEEED-v1
```

The HOBO-only rollback/reference remains:

```text
hobo-mx2001-mx2201-mx2203
```

---

## 2. Critical PIR regression history

During the soil/rock build, bench testing exposed a fatal interaction between the SEN0171 and the Wio-SX1262:

1. the node transmitted LoRa;
2. RF from its own transmission could drive D6 HIGH;
3. the SEN0171/D6 condition could remain HIGH for several seconds;
4. ordinary edge detection could treat that RF-created HIGH as real motion;
5. the motion alert itself caused another LoRa transmission;
6. this could create repeated false motion events / self-triggering behavior.

A short debounce did not solve the problem because the false HIGH could persist longer than the debounce.

### Final 1.0.7 rule

The final fix from the soil/rock branch is now also the source of truth in this plain branch:

- D6 uses the nRF52840 internal `INPUT_PULLDOWN`.
- `RadioLibInterface::instance->isSending()` identifies local LoRa TX.
- A **new** raw D6 HIGH that begins during local TX or during the **15-second post-TX guard** is classified as RF-correlated.
- An RF-correlated HIGH is suppressed for its **entire physical HIGH pulse**.
- Suppression clears only after raw D6 returns LOW.
- Only a fresh LOW→HIGH after that LOW can count as motion.
- A legitimate motion HIGH that began before a later LoRa TX is not invalidated.

The implementation is in:

```text
src/modules/CCAStationModule.h
```

The existing CCA call sites still use `pinMode()` / `digitalRead()`, but this header routes D6 through the RF-aware wrappers. Non-D6 pins retain ordinary Arduino behavior.

### Do not regress these values casually

```text
PIR poll interval:      100 ms
Post-TX RF guard:       15,000 ms
PIR input mode:         INPUT_PULLDOWN
Re-arm requirement:     physical LOW
Valid event:            fresh LOW -> HIGH only
```

The 15-second value was selected after the shorter guard still allowed the observed RF/PIR behavior. Changing it requires a new bench test with real LoRa transmissions.

---

## 3. Shared behavior with the soil/rock branch

The combined rock branch uses the same `CCAStationModule.h` RF filter. Its `CCARockTelemetryModule.cpp` includes the CCA PIR header so its motion flag and motion counter are based on the same validated D6 signal rather than raw `digitalRead(D6)`.

This matters because otherwise the CCA PIR alert could be clean while the soil/rock packet still reported phantom motion.

The rock branch also reverted the experimental design that transmitted a rock packet directly from every PIR edge. Periodic rock telemetry remains separate from PIR alert logic so an RF-induced transition cannot create a transmit-feedback loop.

---

## 4. Wiring

| XIAO pin | Function |
|---|---|
| D0 / A0 | Reserved for soil/rock build; unused here |
| D1 | SX1262 DIO1 |
| D2 | SX1262 RESET |
| D3 | SX1262 BUSY |
| D4 | SX1262 CS |
| D5 | SX1262 RX enable |
| **D6** | **SEN0171 digital signal** |
| D7 | Free |
| D8 | SX1262 SCK |
| D9 | SX1262 MISO |
| D10 | SX1262 MOSI |

SEN0171:

```text
VCC    -> 3V3
GND    -> GND
SIGNAL -> D6
```

GNSS is compiled out of this dedicated target, preventing the normal Seeed GNSS D6 mapping from taking the PIR pin.

---

## 5. PIR event behavior

At boot:

- if D6 is LOW, the PIR can arm;
- if D6 is HIGH, firmware waits for LOW before arming;
- no boot-time HIGH is automatically counted as motion.

For a valid event:

1. filtered D6 transitions LOW→HIGH;
2. persistent total increments;
3. since-boot count increments;
4. last valid detection time is updated;
5. persistent state is written;
6. if `PIR TX ON` and an alert destination exists, a private DM is sent.

Example:

```text
PIR|ALERT|COUNT=17
```

While the physical PIR stays HIGH, that event does not repeat. The next valid event requires LOW and then a fresh HIGH.

---

## 6. Private alert routing

Automatic CCA alerts are **never** allowed to fall back to LongFast broadcast.

Set the receiver by DMing the field node from the intended receiving radio:

```text
ALERTS HERE
```

Check:

```text
ALERTS STATUS
```

Clear from the assigned receiver:

```text
ALERTS CLEAR
```

Persistent automatic messages include:

```text
PIR|ALERT|COUNT=17
SYS|BOOT|COUNT=7|FW=CCA-MX-PIR-1.0.7
POWER|LOW|V=3.590
POWER|CRITICAL|V=3.440
POWER|RECOVERED|V=3.670
```

If no private destination is set, these are suppressed.

---

## 7. Persistence

### Survives reboot

- PIR ON/OFF
- PIR TX ON/OFF
- persistent PIR total
- boot count
- private alert destination
- HOBO lock/assignment through the existing HOBO module

### Resets at reboot

- PIR count since this boot
- `PIR LAST` uptime reference
- power-history ring buffer
- power min/max
- DEBUG state

`PIR RESET` intentionally clears the persistent PIR total.

`POWER RESET` affects only the RAM power-history/min/max state. It does not reboot the node or erase PIR, alert, HOBO, or Meshtastic settings.

---

## 8. Power behavior

Power sampling interval:

```text
10 minutes
```

Thresholds:

```text
LOW:       below 3.60 V
CRITICAL:  below 3.45 V
RECOVERY:  3.65 V hysteresis point
```

History holds roughly 24 hours in RAM. The system tracks current voltage, Meshtastic battery percentage, charging indication, min/max, and approximate 1h/6h/12h/24h references.

The Seeed/Wio-SX1262 actual TX-power limit is **22 dBm** for this hardware target.

---

## 9. HOBO behavior retained

Supported:

```text
MX2001
MX2201
MX2203
```

Commands:

```text
LOGGER
READ
LOCK
UNLOCK
```

The CCA PIR filter does not alter BLE scanning, logger lock state, logger record timing, or HOBO packet formats.

A HOBO BLE problem does not disable the CCA PIR thread.

---

## 10. Complete command list

The root [`README.md`](README.md) contains descriptions. Quick list:

```text
ALERTS HERE
ALERTS STATUS
ALERTS CLEAR

STATUS
VERSION
UPTIME
BOOT
DEBUG ON
DEBUG OFF

PIR
PIR STATUS
PIR COUNT
PIR LAST
PIR RESET
PIR ON
PIR OFF
PIR TX ON
PIR TX OFF

POWER
POWER STATUS
POWER VOLTAGE
POWER MINMAX
POWER TREND
POWER HISTORY
POWER RESET
POWER UPTIME

LOGGER
READ
LOCK
UNLOCK
```

Send CCA commands as direct messages to the node. They are case-insensitive and may use an optional leading `/`.

---

## 11. Build / flash

```powershell
cd C:\Meshtastic\HOBO\firmware
git fetch origin
git switch CCA-MX-HOBO-PIR-SEEED-v1
git pull --ff-only origin CCA-MX-HOBO-PIR-SEEED-v1

.\CCA-MX-HOBO-PIR\build.ps1
.\CCA-MX-HOBO-PIR\flash.ps1
```

Serial monitor:

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
& $pio device monitor -b 115200
```

Runtime identity must be:

```text
CCA-MX-PIR 1.0.7
```

---

## 12. Required RF regression test

After every future PIR-related firmware change:

### A. Idle test

Leave the unit motionless long enough to establish no spontaneous PIR increments.

### B. Real-motion test

After D6 returns LOW, trigger one real motion event. Verify exactly one valid event/private alert.

### C. Self-TX test

With nobody moving, repeatedly DM commands such as:

```text
STATUS
POWER
PIR STATUS
LOGGER
```

These force LoRa replies. Continue observing through the 15-second guard and the SEN0171's physical HIGH hold. `PIR COUNT` must not increase because of the node's own transmissions.

### D. Recovery test

After any RF-induced raw HIGH has physically returned LOW, real motion must be detectable again.

### E. Persistence test

Power-cycle and verify:

```text
VERSION
ALERTS STATUS
PIR COUNT
BOOT
LOGGER
```

Do not call a future PIR build deployable until the self-TX test passes.
