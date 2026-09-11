# Distance Sensor Module

This folder is the planned shared home for **field-configurable distance sensing** on both supported field platforms:

```text
Seeed XIAO nRF52840 + Wio-SX1262
RAK4631 + RAK19007
```

The goal is not to create one firmware for water and a completely different firmware for trails.

The goal is to build **one distance-sensor subsystem** with:

- interchangeable sensor drivers;
- a shared calibration system;
- a water mode;
- a trail-counter mode;
- persistent settings;
- Meshtastic direct-message configuration;
- common health and recovery behavior inherited from `field-self-recovery`.

This document is written so someone who understands basic electronics but has never seen this project can follow the design.

---

# 1. Current status

This directory currently represents the **design/scaffold stage** of the distance subsystem.

The recovery/power foundation is already working in the parent branch, but the full production distance drivers and application logic are the next implementation task.

Do not assume a command described below is already compiled into a production image until the distance branches show the implementation and successful hardware tests.

Development branches:

```text
distance-self-recovery-seeed
distance-self-recovery-rak4631
```

Canonical parent:

```text
field-self-recovery
```

---

# 2. First supported sensors

The project begins with the three sensor models already available for physical bench testing.

| Sensor | Interface | First intended role |
|---|---|---|
| DFRobot SEN0590 | I2C | fast trail/distance testing |
| DFRobot SEN0311 / A02YYUW | UART | general ultrasonic distance, trail, shorter water setups |
| DFRobot SEN0313 / A01NYUB | UART | longer-range ultrasonic water/stage work |

The exact sensor range, filtering, no-echo behavior, and update rate should remain properties of the driver rather than being hard-coded into the water/trail application logic.

---

# 3. Supported board/sensor combinations

The design target is all six combinations:

| Board | SEN0590 | SEN0311 / A02YYUW | SEN0313 / A01NYUB |
|---|---|---|---|
| Seeed XIAO nRF52840 + Wio-SX1262 | supported target | supported target | supported target |
| RAK4631 + RAK19007 | supported target | supported target | supported target |

That means field crews should not have to learn a different calibration philosophy for Seeed versus RAK.

The board layer selects pins and interfaces.

The shared application layer handles stage or counting.

---

# 4. Architecture

The intended structure is:

```text
DistanceSensorModule
│
├── configuration
│   ├── selected mode
│   ├── selected sensor
│   ├── calibration values
│   ├── trigger settings
│   └── persistent counters
│
├── water application
│   ├── raw distance
│   ├── stage calibration
│   ├── derived stage
│   └── water QA/QC
│
├── trail application
│   ├── clear-path calibration
│   ├── baseline
│   ├── blocked/clear state machine
│   ├── hysteresis
│   ├── fast rearm
│   ├── daily count
│   └── lifetime count
│
└── drivers
    ├── SEN0590
    ├── SEN0311 / A02YYUW
    └── SEN0313 / A01NYUB
```

The shared code should ask a driver something conceptually like:

```text
read distance
```

The driver can then do whatever hardware-specific work is required.

The water or trail code should not care whether the measurement came from I2C or UART.

---

# 5. Why sensor drivers are separated

Different sensors speak different electronic languages.

For example:

```text
SEN0590
  ↓
I2C request/response

SEN0311 / SEN0313
  ↓
UART serial frames
```

They may also differ in:

- measurement range;
- update rate;
- invalid-value codes;
- startup time;
- filtering behavior;
- no-target behavior;
- power requirements.

Those differences belong in the **driver**.

The rest of the firmware should receive a normalized result such as:

```text
valid: true
distance_mm: 1842
quality: OK
```

or:

```text
valid: false
error: NO_ECHO
```

That makes it possible to add another sensor later without rewriting the trail counter.

---

# 6. Planned board wiring philosophy

The project should standardize pins rather than choosing random GPIOs for every node.

## Seeed

Target interfaces:

```text
I2C sensor:
  SDA → standard XIAO I2C SDA
  SCL → standard XIAO I2C SCL

UART sensor:
  sensor TX → XIAO UART RX
```

If a sensor streams data and does not require commands, a receive-only UART connection can be enough for the first implementation.

## RAK19007

Target interfaces:

```text
I2C sensor:
  use standard WisBlock I2C bus

UART sensor:
  sensor TX → RAK4631 UART RX exposed through RAK19007
```

Exact connector/pin documentation should be finalized and tested before field deployment.

The architecture should permit future power-control GPIOs if a sensor can be switched off between measurements.

---

# 7. Operating modes

The planned top-level modes are:

```text
MODE WATER
MODE TRAIL
```

Mode controls how a valid distance reading is interpreted.

The underlying driver does not change its job.

A distance is still a distance.

What changes is what the firmware does with it.

---

# 8. Water mode

## 8.1 What the sensor measures

An overhead distance sensor measures:

```text
sensor → water surface
```

Call this:

```text
D = raw distance
```

That is not automatically stage.

## 8.2 Field calibration

The operator mounts the sensor first.

Then the operator measures the actual water stage using a tape, staff gauge, or another trusted reference.

Example:

```text
raw sensor distance = 6.83 ft
known stage         = 1.42 ft
```

Command:

```text
CAL STAGE 1.42FT
```

The firmware calculates:

```text
reference = raw distance + known stage
reference = 6.83 + 1.42
reference = 8.25 ft
```

That reference is saved.

Future reading:

```text
raw distance = 5.83 ft
reference    = 8.25 ft

stage = reference - raw distance
stage = 2.42 ft
```

## 8.3 Why this calibration method is important

The field crew does not need to know the final sensor mounting height before installation.

The sensor can be mounted where:

- the tree is stable;
- the bridge beam is accessible;
- the beam has a clear view of water;
- flood debris is less likely to strike it;
- maintenance access is practical.

After mounting, one trusted stage measurement defines the geometry.

## 8.4 Calibration sampling

`CAL STAGE` should not trust one noisy distance sample if the driver can provide multiple samples.

Preferred behavior:

```text
collect several valid readings
reject obvious invalid values
use a stable representative value such as median
combine with known stage
save reference
```

The response should show the operator exactly what was saved.

Example:

```text
CALIBRATION SAVED
Sensor: SEN0313
Raw distance used: 6.830 ft
Known stage:       1.420 ft
Reference:         8.250 ft
Samples:           15
Spread:            0.018 ft
```

## 8.5 Raw measurement retention

Telemetry should retain:

```text
raw distance
calculated stage
calibration reference
```

The raw distance must never be silently replaced by stage.

## 8.6 Missing calibration

If the node is in water mode but has not been calibrated, it should still be able to report raw distance.

It should not invent stage.

Example health state:

```text
RAW: 1842 mm
STAGE: unavailable
STATUS: CALIBRATION_MISSING
```

---

# 9. Water QA/QC

Potential checks include:

## No echo / no target

If the ultrasonic sensor does not return a valid target, report:

```text
NO_ECHO
```

Do not reuse the previous stage as though it were a new reading.

## Out of range

If a value falls outside the valid range for that sensor model:

```text
OUT_OF_RANGE
```

## Excessive sample spread

If a calibration set or normal measurement set varies too widely, flag it:

```text
NOISY
```

## Implausible jump

A sudden large stage change may be real during a flash flood, so the firmware should be careful about rejecting it.

The preferred approach is:

```text
preserve raw reading
flag questionable jump
let downstream QA/QC decide whether it is real
```

This is safer than deleting unusual hydrologic events automatically.

---

# 10. Trail mode

Trail mode turns a stream of distance measurements into discrete pass events.

The sensor is mounted on one side of the trail and looks across it.

Example:

```text
sensor  ---------------------------------- opposite side
                empty trail
```

The empty-trail reading becomes the baseline.

---

# 11. `CAL CLEAR`

The preferred field command is:

```text
CAL CLEAR
```

The operator makes sure the trail is empty.

The firmware takes many clear-path samples.

Example samples:

```text
1575 mm
1578 mm
1577 mm
1576 mm
1579 mm
```

The firmware saves a stable baseline, for example:

```text
baseline = 1577 mm
```

This is better than typing only the tape-measured trail width because it reflects the actual sensor beam and mounting angle.

A manual baseline command may still be useful for testing, but `CAL CLEAR` should be the normal field workflow.

---

# 12. Trail trigger logic

The counter should not count every reading slightly below baseline.

Instead:

```text
trigger threshold = baseline - trigger_delta
```

Example:

```text
baseline      = 1577 mm
trigger delta = 305 mm  (~12 in)
trigger point = 1272 mm
```

Only a substantial intrusion into the beam should start a pass event.

---

# 13. Hysteresis

The blocked threshold and clear threshold should be different.

Example:

```text
baseline:       1577 mm
blocked below:  1272 mm
clear above:    1425 mm
```

Why?

Without hysteresis, noisy readings near one threshold could cause:

```text
BLOCKED
CLEAR
BLOCKED
CLEAR
BLOCKED
```

for one person.

Hysteresis makes the state machine stable.

---

# 14. Trail state machine

The intended logic is approximately:

```text
STATE: CLEAR
    │
    │ several consecutive blocked samples
    ▼
STATE: BLOCKED
    │
    │ count event exactly once
    ▼
STATE: WAITING_FOR_CLEAR
    │
    │ several consecutive clear samples
    ▼
STATE: CLEAR
```

The system counts the **transition/event**, not every raw sample.

This means:

```text
person blocks beam for 0.3 s → one count
person blocks beam for 5 s   → one count
```

The node must not count 50 people just because one person stood still in front of a 10 Hz sensor.

---

# 15. Fast rearm

The project specifically wants to avoid the long fixed hold times seen with earlier PIR trail testing.

The distance counter should rearm when the beam actually clears.

Target behavior:

```text
blocked
  ↓
person leaves beam
  ↓
2–3 valid clear samples
  ↓
armed again
```

With a sufficiently fast sensor and sensible filtering, rearm may be possible in a few hundred milliseconds.

The exact value should be proven on the bench with real passes rather than guessed.

---

# 16. Consecutive-sample confirmation

One noisy measurement should not create a person.

A reasonable initial design is:

```text
require 2 or 3 blocked samples before declaring BLOCKED
require 2 or 3 clear samples before declaring CLEAR
```

The exact number may be sensor-specific because SEN0590 and the ultrasonic sensors can have different update behavior.

This is a good example of why the shared state machine should accept driver timing information instead of assuming every sensor behaves identically.

---

# 17. Obstruction handling

A permanent obstruction is different from a person passing through.

Possible causes:

```text
fallen branch
snow
vegetation
animal standing nearby
rock or debris
sensor knocked out of alignment
```

If the beam stays blocked for an unusually long time, the firmware should flag:

```text
OBSTRUCTION
```

It should not silently redefine the obstruction as the new baseline.

Automatic silent recalibration would make bad data look good.

---

# 18. Counters

Trail mode should keep at least:

```text
count_today
count_total
```

Potential future fields:

```text
count_since_boot
last_event_time
last_event_duration
minimum_distance_last_event
maximum_events_per_minute
```

Persistent counters must survive normal reboot and battery loss.

Counter storage should be designed carefully so flash is not written excessively on every high-rate sensor sample.

---

# 19. Local sampling versus LoRa transmission

Trail detection may require fast local sampling.

Example:

```text
10–20 sensor samples per second
```

That does **not** mean 10–20 LoRa packets per second.

LoRa airtime is limited and shared with the mesh.

The correct model is:

```text
fast local sampling
       ↓
local event detection
       ↓
update local counters
       ↓
transmit compact event or periodic summary
```

Water mode can normally sample and transmit much more slowly.

This distinction is essential for a scalable network.

---

# 20. Planned commands

The exact parser is not final, but the desired operator experience is:

## General

```text
HELP
STATUS
HEALTH
VERSION
POWER
BATTERY
SENSOR
RAW
READ
STATS
RECOVER
REBOOT
```

## Mode

```text
MODE
MODE WATER
MODE TRAIL
```

## Sensor

Possible configuration forms:

```text
SENSOR
SENSOR SEN0590
SENSOR SEN0311
SENSOR SEN0313
```

Automatic detection may be possible for some hardware, but explicit sensor selection must remain available when two models use similar interfaces/protocols.

## Water

```text
CAL
CAL STAGE 1.42FT
CAL STAGE 17IN
CAL RESET
STAGE
INTERVAL 15MIN
```

## Trail

```text
CAL CLEAR
BASELINE
TRIGGER 12IN
COUNT
COUNT TODAY
COUNT TOTAL
RESET COUNT
```

---

# 21. Unit handling

Field users should be able to enter human-friendly units.

Potential accepted forms:

```text
1.42FT
1.42 FT
17IN
17 IN
430MM
43CM
```

Internally, the firmware should use one consistent unit, preferably integer millimeters where practical.

Example:

```text
user enters: 1.42 ft
parser converts to: 433 mm
saved calibration uses: mm
reply can display: ft + mm if desired
```

Using a single internal unit reduces floating-point and conversion mistakes.

---

# 22. Persistent configuration

Distance configuration should survive:

```text
normal reboot
watchdog reset
manual RECOVER
battery disconnect
low-battery SYSTEM OFF
solar recharge wake
normal firmware update
```

Potential saved structure includes:

```text
schema_version
mode
sensor_type
water_reference_mm
trail_baseline_mm
trail_trigger_delta_mm
trail_clear_delta_mm
sample_confirmation_count
count_today
count_total
last_reset_date/time if valid clock exists
```

A future versioned schema is strongly preferred so firmware updates can migrate old settings safely.

---

# 23. Data integrity

## Preserve raw data

Every derived water value should be traceable back to raw distance.

## Do not fabricate missing values

If the sensor fails:

```text
sensor = failed
```

is better than copying the previous good value and pretending it is new.

## Flag unusual values

A flash flood can create a real extreme change.

Do not automatically delete unusual hydrologic data just because it looks surprising.

## Keep calibration visible

Remote `STATUS` or `CAL` should be able to report the active calibration/reference so a field operator can audit what the node is doing.

---

# 24. Telemetry examples

## Water packet concept

```text
node: Creek A
mode: water
sensor: SEN0313
raw_distance_mm: 1777
stage_mm: 738
reference_mm: 2515
sensor_status: OK
battery_mv: 3970
battery_percent: 72
observed_at: <timestamp>
```

## Trail packet concept

```text
node: Trail Counter 1
mode: trail
sensor: SEN0590
count_today: 37
count_total: 1842
baseline_mm: 1577
last_event_min_mm: 820
sensor_status: OK
battery_mv: 4010
battery_percent: 78
observed_at: <timestamp>
```

Exact protobuf/custom payload format will be decided during implementation.

---

# 25. Recovery inheritance

Distance firmware should reuse the parent branch's recovery behavior rather than implementing another watchdog or another solar system.

Inherited concepts include:

```text
hardware watchdog
safe reboot
battery telemetry
Seeed solar wake
RAK comparator wake
low-voltage safety margin
persistent settings
non-destructive update rules
remote health diagnostics
```

If the distance module starts directly writing watchdog registers or erasing configuration, the layer separation has been violated.

---

# 26. Bench-test plan

A distance build should not be called field-ready until it passes real sensor tests.

## Sensor basics

```text
sensor disconnected at boot
sensor connected at boot
sensor unplugged while running
sensor plugged back in
invalid frame/no echo
maximum practical distance
minimum practical distance
```

## Water calibration

```text
CAL STAGE with known distance
reboot
verify calibration retained
change simulated water level
verify stage math
no echo
noisy water surface
extreme but valid stage change
CAL RESET
```

## Trail counting

```text
one slow walker
one fast walker
two people close together
person stops in beam
person backs out
small object/noise
vegetation movement
permanent obstruction
reboot with saved count
battery cycle with saved count
```

## Recovery

```text
manual RECOVER
watchdog reset if safely testable
battery removal
low-voltage behavior
solar recharge wake
```

## Network

```text
local sensor read
Meshtastic transmission
relay path
gateway receipt
cloud/database receipt when enabled
```

---

# 27. Field installation examples

## Water

```text
1. Mount sensor securely above water.
2. Confirm beam is not hitting bridge structure, branches, or bank.
3. Power node.
4. DM STATUS.
5. DM SENSOR.
6. DM MODE WATER.
7. DM RAW.
8. Measure true stage with tape/staff gauge.
9. DM CAL STAGE <value>.
10. DM READ.
11. Compare returned stage with field measurement.
12. Reboot/recover once.
13. Verify calibration persisted.
14. Verify remote telemetry path.
```

## Trail

```text
1. Mount sensor at chosen height and angle.
2. Make sure opposite side produces a stable clear return.
3. Power node.
4. DM STATUS.
5. DM SENSOR.
6. DM MODE TRAIL.
7. Clear the trail.
8. DM CAL CLEAR.
9. Walk through the beam repeatedly.
10. Verify one count per pass.
11. Test two close passes.
12. Stand in the beam and confirm only one count.
13. Clear the beam and confirm quick rearm.
14. Reboot.
15. Confirm baseline and counts persisted.
16. Verify remote telemetry path.
```

---

# 28. Design rule for future sensors

When adding another distance sensor, the preferred question is:

> **Can this new sensor be made to return the same normalized distance/error structure to the shared application?**

If yes, add a new driver.

Do not create a new copy of `WaterStage.cpp` or `TrailCounter.cpp` just because the wire protocol changed.

That is the central maintainability goal of this subsystem.
