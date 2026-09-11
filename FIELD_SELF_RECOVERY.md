# Canonical Field Self-Recovery Baseline

This document is the technical reference for the `field-self-recovery` branch.

The root [`README.md`](README.md) is the repository front page and explains the project in broad terms. This file goes deeper into **why the recovery system exists, how it behaves, what is safe to change, and what every future field-sensor branch is expected to inherit**.

---

# 1. Purpose

A remote field sensor is useful only if it remains alive long enough to collect and transmit data.

The project therefore treats these features as part of the basic platform:

- watchdog recovery;
- safe reboot;
- low-battery behavior;
- solar recharge wake;
- BLE recovery;
- persistent configuration;
- battery telemetry;
- board-specific power handling;
- non-destructive firmware updates;
- remote diagnostics.

A sensor driver should not have to reinvent any of those systems.

The baseline goal is:

> **A field node should recover from ordinary software, power, and connection failures without erasing the information that makes it that specific station.**

---

# 2. Canonical branch

The canonical development branch is:

```text
field-self-recovery
```

This is the repository default branch.

Future unattended nRF52 sensor projects should normally branch from this line.

Examples:

```text
field-self-recovery
        │
        ├── distance-self-recovery-seeed
        ├── distance-self-recovery-rak4631
        ├── future-soil-node
        ├── future-weather-node
        └── future-flood-node
```

The older HOBO recovery branches remain valuable because they are the validated source lines from which this common baseline was assembled.

---

# 3. Supported board families

## 3.1 Seeed

Primary platform:

```text
Seeed XIAO nRF52840
+
Wio-SX1262
```

The XIAO provides the nRF52840 microcontroller.

The Wio-SX1262 provides the LoRa radio.

Important recovery-specific characteristics:

- nRF52840 watchdog available;
- nRF52840 LPCOMP available;
- battery sensed through the XIAO battery divider;
- custom comparator configuration added for solar recharge wake;
- low end of configured battery curve raised to approximately 3.40 V.

## 3.2 RAK

Primary new field platform:

```text
RAK4631
+
RAK19007
```

The RAK4631 contains:

- nRF52840 microcontroller;
- SX1262 LoRa radio.

The RAK19007 is the baseboard standardized for new distance-sensor deployments.

Existing Meshtastic RAK4631 support may work on other WisBlock bases, but project wiring and new distance-node documentation should assume RAK19007 unless a branch explicitly says otherwise.

Important recovery-specific characteristics:

- nRF52840 watchdog available;
- RAK board definition already contains battery LPCOMP wake support;
- canonical baseline retains that wake path;
- safer low-voltage OCV bottom is approximately 3.40 V.

---

# 4. Recovery hierarchy

Recovery should escalate from smallest intervention to largest intervention.

Conceptually:

```text
healthy node
   │
   ├── no action
   │
minor interface problem
   │
   ├── refresh scanner / interface
   │
long stale connection
   │
   ├── safe software reboot
   │
firmware completely stuck
   │
   ├── hardware watchdog reset
   │
low battery
   │
   ├── safe power-off / deep sleep
   │
solar restores battery
   │
   └── comparator wake → normal boot
```

The system should not jump directly from “sensor did not answer once” to “erase the radio.”

---

# 5. Hardware watchdog

## 5.1 What it is

The watchdog is built into the nRF52840.

The recovery supervisor periodically writes the watchdog reload value.

If the firmware becomes stuck long enough that it stops feeding the watchdog, the chip resets itself.

## 5.2 Current timeout

The self-recovery implementation uses approximately:

```text
15 minutes
```

This is intentionally long.

Remote field firmware can spend substantial time sleeping or waiting for radios/sensors, so the watchdog should catch a genuine lockup rather than interfere with normal power-saving behavior.

## 5.3 Run during sleep

The watchdog is configured to continue running while the CPU sleeps.

This prevents a broken sleep state from becoming a permanent lockup.

## 5.4 Ownership rule

The supervisor checks whether the watchdog is already running.

If another part of the firmware already owns it, self-recovery does not blindly overwrite the existing configuration.

That avoids two unrelated modules fighting over the same hardware peripheral.

---

# 6. Safe remote reboot

The direct-message recovery commands are intended to provide a full microcontroller restart without changing station identity or settings.

Current commands include:

```text
RECOVER
REBOOT
```

The expected behavior is:

```text
receive command
      ↓
reply to sender
      ↓
brief delay so reply can be queued
      ↓
NVIC_SystemReset()
      ↓
normal boot
```

The reboot is not a factory reset.

It must preserve NVS and field configuration.

---

# 7. BLE disconnected-state recovery

The HOBO system uses BLE, so BLE recovery was one of the first major field requirements.

## 7.1 Do not disturb a healthy link

If the scanner is not running because the firmware appears to have an active/idle connection state, the recovery supervisor avoids restarting it unnecessarily.

The system should not destroy a good logger connection just to prove the recovery code is active.

## 7.2 Low-duty disconnected scan

When disconnected, the scanner is tuned approximately to:

```text
interval: 200 ms
window:    20 ms
passive:   yes
```

That is roughly a 10% scan duty cycle.

The purpose is to keep searching without leaving the BLE receiver fully active all the time.

## 7.3 Scanner refresh

The disconnected scanner can be restarted periodically to clear a stale scanner state.

Current design target:

```text
about every 30 minutes while disconnected
```

## 7.4 Long-disconnect reboot

If the node has remained in the disconnected scanning condition for approximately six hours, the recovery supervisor schedules a safe reboot.

That rebuilds the BLE stack and overall runtime state.

This is intentionally much slower than the scanner-refresh interval.

The escalation path is therefore:

```text
scan
  ↓
refresh scanner
  ↓
continue waiting
  ↓
only after very long failure → reboot
```

---

# 8. Seeed solar recovery

## 8.1 The original field problem

A solar-powered Seeed node could enter nRF52 SYSTEM OFF after low battery.

The battery could later recharge in sunlight while the processor remained asleep because the board had no configured battery-rise LPCOMP input in the project variant.

USB insertion could wake the unit, which made the symptom look like a mysterious firmware freeze even though the root problem was the deep-power wake path.

## 8.2 Battery input

The Seeed variant uses the battery ADC through:

```text
P0.31 / AIN7
```

with the board's battery divider.

## 8.3 Added LPCOMP configuration

The canonical variant configures:

```text
BATTERY_LPCOMP_INPUT = NRF_LPCOMP_INPUT_7
BATTERY_LPCOMP_THRESHOLD = NRF_LPCOMP_REF_SUPPLY_3_8
```

The project comments estimate this as roughly a **3.67 V battery recovery point** under the assumed divider/rail conditions.

It should be understood as an approximate engineering threshold.

## 8.4 Safer low end

The Seeed OCV table is configured with approximately:

```text
3.40 V = bottom of battery curve
```

The purpose is to stop treating much lower voltage as a normal operating region.

## 8.5 Desired power cycle

```text
normal operation
      ↓
battery declines
      ↓
low-voltage shutdown / SYSTEM OFF
      ↓
solar charges battery
      ↓
AIN7 crosses LPCOMP recovery threshold
      ↓
processor wakes
      ↓
normal Meshtastic boot
```

---

# 9. RAK solar recovery

The RAK4631 variant already contains battery comparator definitions.

Important values include:

```text
BATTERY_LPCOMP_INPUT = NRF_LPCOMP_INPUT_3
BATTERY_LPCOMP_THRESHOLD = NRF_LPCOMP_REF_SUPPLY_11_16
```

The source comments estimate the selected comparator point at roughly **3.76 V battery** under the documented divider assumptions.

The canonical baseline also adjusts the bottom of the RAK OCV curve to approximately 3.40 V.

Conceptually:

```text
shutdown region ≈ 3.40 V end of configured curve
recovery region ≈ substantially higher battery voltage
```

That separation helps prevent low-voltage boot cycling.

---

# 10. Why hysteresis matters for power

A system with identical shutoff and wake thresholds can chatter.

Example of a bad design:

```text
3.40 V → power off
3.41 V → wake
3.39 V → power off
3.41 V → wake
```

Each boot consumes energy.

A weak solar panel could trap the station in repeated boot attempts.

A higher recovery threshold allows the battery to accumulate enough energy before the node restarts.

This is the same general engineering idea later used in trail-counter trigger/clear thresholds.

---

# 11. Persistence requirements

A reset is acceptable only if the station still knows who and what it is afterward.

Persistent information includes, where applicable:

```text
Meshtastic node identity
Meshtastic channels
PSKs / encryption keys
NodeDB
radio configuration
logger lock
distance sensor type
water calibration
trail baseline
trail trigger configuration
daily/lifetime counter state
future field settings
```

A recovery feature that deletes those values creates a new problem while fixing the old one.

---

# 12. NVS safety

NVS means non-volatile storage.

Normal recovery code must not erase it.

Normal firmware updates must not require erasing it.

The project rule is:

> **No `erase-flash` for routine field-node firmware updates.**

Factory images or full flash erases belong only to deliberate destructive recovery when the operator has decided that preserving the existing configuration is no longer possible or desirable.

---

# 13. Battery telemetry

Recovery decisions are much easier when the node reports its own power condition.

The project therefore treats battery telemetry as a baseline feature.

Useful fields include:

```text
battery voltage
battery percent
battery present
charging state when available
```

A future remote diagnostic should make it possible to distinguish:

```text
sensor failed but battery is healthy
```

from:

```text
entire node is unstable because battery is collapsing
```

Those are very different maintenance decisions.

---

# 14. Existing direct-message diagnostics

The self-recovery supervisor includes commands intended for remote field diagnosis.

Current command family includes:

```text
STATUS
HEALTH
POWER
BATTERY
BLE
AUTO
STATS
NODES
UPTIME
VERSION
WATCHDOG
SCAN
RECONNECT
RECOVER
REBOOT
PING
WAKE
HELP
```

These are separate from sensor-specific commands such as HOBO `READ`, `LOGGER`, `LOCK`, and `UNLOCK`.

The architectural point is that **platform health commands belong to the recovery layer**, while sensor-specific commands belong to the sensor layer.

Future distance firmware should preserve that separation.

---

# 15. HOBO integration

The current field baseline was validated while supporting:

```text
MX2001
MX2201
MX2203
```

The HOBO protocol layer remains separate from recovery.

The recovery supervisor should not rewrite the proven logger decoding/state machine.

That design allows a later sensor such as SEN0313 to reuse the recovery foundation without importing HOBO-specific assumptions.

---

# 16. Distance-sensor inheritance

The new distance branches are:

```text
distance-self-recovery-seeed
distance-self-recovery-rak4631
```

They start from the canonical field baseline.

That means the distance implementation should inherit:

```text
watchdog
safe reboot
battery telemetry
solar handling
persistent configuration principles
non-destructive updates
remote health commands
board-specific power definitions
```

The new code should add only what is genuinely distance-specific:

```text
sensor drivers
raw distance acquisition
water-stage calibration
trail baseline calibration
trail event state machine
sensor-specific error handling
new telemetry schema
```

---

# 17. Board-specific code versus shared code

A core project rule is to keep board-specific details near the board layer.

Examples of board-specific concerns:

```text
which UART pins are used
which I2C bus is used
which battery ADC exists
which LPCOMP input maps to battery
which GPIO can control sensor power
```

Examples of shared concerns:

```text
stage = reference - raw_distance
trail count state machine
hysteresis
persistent calibration format
QA/QC flags
telemetry field names
command parser semantics
```

If the same stage equation appears in separate Seeed and RAK files, the architecture is drifting in the wrong direction.

---

# 18. CI requirements

The canonical branch contains a GitHub Actions workflow that builds both major nRF52 targets.

Required targets:

```text
seeed_xiao_nrf52840_kit
rak4631
```

A shared baseline change should not be considered ready until both compile successfully.

The purpose of this rule is simple:

```text
shared code change
      ↓
Seeed build passes
RAK build fails
      ↓
change is NOT ready
```

The repository uses CI to catch this before deployment.

---

# 19. Recovery testing philosophy

A build compiling is necessary, but it is not enough.

A field-ready recovery change should eventually be tested against real failure modes.

Examples:

```text
normal reboot
watchdog reset
battery removed and restored
battery drained into low-voltage state
solar recharge after deep power-off
BLE logger temporarily removed
BLE logger restored
scanner refresh
long disconnect
manual RECOVER
sensor disconnected
sensor reconnected
firmware update without erasing NVS
```

The expected outcome is not merely that the processor boots.

The station should also retain its identity, settings, and field calibration.

---

# 20. What this branch is not

`field-self-recovery` is not meant to become one huge application containing every experimental sensor feature.

It is the common foundation.

Application-specific work belongs on child branches until it is mature enough to become shared infrastructure.

That keeps the baseline understandable and reduces the chance that an experimental trail feature breaks a production HOBO node.

---

# 21. Source branches incorporated

Primary validated recovery source lines:

```text
hobo-self-recovery-seeed
hobo-self-recovery-rak4631
```

These branches remain useful references for comparing board-specific behavior.

The canonical branch combines their important platform-level protections and builds both board targets.

---

# 22. Future baseline improvements

Potential future improvements that fit naturally in this shared layer include:

```text
standard persistent config schema/versioning
CRC or integrity check for saved field config
reset-reason history
brownout/recovery counters
sensor-power-cycle helper
common remote diagnostic formatting
common event log / compact health history
admin-node authorization helper for configuration commands
```

Those features should be implemented once in the baseline if they are useful to multiple sensor applications.

---

# 23. Final rule

For a new unattended sensor project, ask this question first:

> **Is this feature specific to the sensor, or is it something every remote field node should know how to do?**

If it is sensor-specific, put it in the sensor/application layer.

If it is a general survival, battery, recovery, persistence, or diagnostics feature, it probably belongs in `field-self-recovery`.

That separation is what will keep the project maintainable as the network grows from a few radios into many remote monitoring stations.
