# Meshtastic Field Self-Recovery Firmware

> **Default branch:** `field-self-recovery`
>
> **Main idea:** build remote Meshtastic sensor nodes that can stay outside for months, survive battery and solar problems, recover from software or BLE failures, keep their settings, and be diagnosed from a distance instead of requiring a hike back to the site.

This repository is a custom Meshtastic firmware tree for remote environmental monitoring, trail counting, water-level sensing, HOBO logger telemetry, and other unattended field sensors.

The repository is intentionally organized around one simple rule:

> **Every new field sensor should inherit the same proven recovery, battery, solar, watchdog, and remote-diagnostics foundation instead of starting from scratch.**

The current default branch, `field-self-recovery`, is that foundation.

---

## Table of contents

1. [What this repository does](#what-this-repository-does)
2. [A 60-second explanation](#a-60-second-explanation)
3. [What is already working and what is still planned](#what-is-already-working-and-what-is-still-planned)
4. [Hardware this project is standardized around](#hardware-this-project-is-standardized-around)
5. [How the whole system fits together](#how-the-whole-system-fits-together)
6. [Why self-recovery matters](#why-self-recovery-matters)
7. [Watchdog recovery](#watchdog-recovery)
8. [Solar and low-battery recovery](#solar-and-low-battery-recovery)
9. [BLE recovery](#ble-recovery)
10. [What recovery is allowed to change](#what-recovery-is-allowed-to-change)
11. [Current HOBO support](#current-hobo-support)
12. [Distance-sensor project](#distance-sensor-project)
13. [Water-level mode](#water-level-mode)
14. [Trail-counter mode](#trail-counter-mode)
15. [Supported distance sensors](#supported-distance-sensors)
16. [Planned direct-message commands](#planned-direct-message-commands)
17. [Telemetry and data-integrity rules](#telemetry-and-data-integrity-rules)
18. [Branch map](#branch-map)
19. [Source-code layout](#source-code-layout)
20. [Build and CI rules](#build-and-ci-rules)
21. [Safe firmware-update rules](#safe-firmware-update-rules)
22. [Field deployment workflow](#field-deployment-workflow)
23. [Troubleshooting philosophy](#troubleshooting-philosophy)
24. [Glossary](#glossary)
25. [Current development status](#current-development-status)

---

# What this repository does

Meshtastic normally turns small LoRa radios into a low-power mesh network. This repository extends that idea so a radio can also act as a **remote scientific field station**.

A field node might be attached to:

- an Onset HOBO MX2001 water-level logger;
- an Onset HOBO MX2201 or MX2203 temperature logger;
- a distance sensor looking down at a creek;
- a distance sensor looking across a trail;
- a future soil-moisture, weather, flood, or other environmental sensor.

The radio reads the sensor, keeps track of its own health and battery, and sends useful information through the Meshtastic mesh.

The difficult part is not taking one reading on a workbench. The difficult part is making the system keep working after it has been outside for weeks or months.

A real field node has to survive things such as:

- the battery getting low overnight;
- several cloudy days;
- solar recharge after the radio has shut down;
- a BLE connection getting stuck;
- a sensor disappearing temporarily;
- a software task hanging;
- a watchdog reset;
- a normal firmware update;
- loss and restoration of power;
- poor radio paths;
- a sensor being moved or recalibrated after installation.

That is why this repository now treats **recovery and persistent configuration as core infrastructure**, not as optional extras.

---

# A 60-second explanation

Think of a remote node as four layers:

```text
┌────────────────────────────────────────────┐
│  1. SENSOR                                │
│  HOBO, distance sensor, future instrument │
└──────────────────────┬─────────────────────┘
                       │ reading
                       ▼
┌────────────────────────────────────────────┐
│  2. FIELD-NODE LOGIC                       │
│  calibration, counting, stage, QA/QC       │
└──────────────────────┬─────────────────────┘
                       │ useful data
                       ▼
┌────────────────────────────────────────────┐
│  3. SELF-RECOVERY FOUNDATION               │
│  watchdog, battery, solar wake, BLE repair │
│  persistent settings, remote diagnostics   │
└──────────────────────┬─────────────────────┘
                       │ Meshtastic packet
                       ▼
┌────────────────────────────────────────────┐
│  4. LORA / MESHTASTIC NETWORK              │
│  field node → relays → gateway → cloud     │
└────────────────────────────────────────────┘
```

The sensor can change without rewriting the recovery system.

The field application can change without rewriting the radio network.

That separation is the main architecture of this project.

---

# What is already working and what is still planned

It is important to distinguish **working production code** from **planned distance-sensor features**.

## Implemented and validated now

The `field-self-recovery` baseline currently contains the recovery work developed and validated on both major nRF52840 field platforms used in this project:

- Seeed XIAO nRF52840 + Wio-SX1262;
- RAK4631;
- nRF52840 hardware watchdog supervision;
- safe remote reboot/recovery commands;
- BLE disconnected-state recovery logic;
- low-duty BLE scanning while disconnected;
- long-disconnect automatic reboot logic;
- battery/device telemetry support;
- persistent field settings;
- safer low-voltage handling for solar nodes;
- Seeed battery-rise wake from deep SYSTEM OFF after solar recharge;
- RAK battery comparator wake support;
- non-destructive recovery that preserves Meshtastic configuration;
- dual-board GitHub Actions builds for Seeed and RAK;
- universal HOBO MX2001, MX2201, and MX2203 support inherited from the validated HOBO work.

## Designed but not yet fully implemented

The new `DistanceSensor` subsystem has been scaffolded and documented, but the complete production driver/application code is still the next development step.

Planned first-generation support is for:

- DFRobot SEN0590;
- DFRobot SEN0311 / A02YYUW;
- DFRobot SEN0313 / A01NYUB;
- water-level/stage mode;
- trail-counter mode;
- direct-message field calibration;
- persistent water calibration;
- persistent trail baseline;
- fast trail rearm;
- local daily and lifetime counts;
- distance-sensor health reporting;
- compact Meshtastic telemetry.

This README deliberately describes both the **working foundation** and the **design target** so someone new to the repository can understand where the project is going without assuming unfinished features already exist.

---

# Hardware this project is standardized around

## Seeed field node

```text
Seeed XIAO nRF52840
        +
Wio-SX1262 LoRa radio
```

The XIAO contains the nRF52840 microcontroller. The Wio-SX1262 provides the LoRa radio used by Meshtastic.

This is a very small, lightweight option for remote sensors.

For the recovery baseline, the Seeed build also includes special battery-comparator configuration so the microcontroller can wake after solar charging raises the battery voltage again.

## RAK field node

```text
RAK4631
   │
   └── mounted on RAK19007 baseboard
```

For **new distance-sensor deployments**, this project standardizes on the **RAK19007** baseboard.

That wording matters:

- **RAK4631** is the radio/core module containing the nRF52840 and SX1262.
- **RAK19007** is the WisBlock baseboard the RAK4631 plugs into.

Older or other supported WisBlock bases may still run ordinary RAK4631 Meshtastic firmware, but new distance-sensor documentation and wiring in this project should assume **RAK4631 + RAK19007** unless explicitly stated otherwise.

## Supported distance combinations planned for this project

| Board | SEN0590 | SEN0311 / A02YYUW | SEN0313 / A01NYUB |
|---|---|---|---|
| Seeed XIAO nRF52840 + Wio-SX1262 | Yes | Yes | Yes |
| RAK4631 + RAK19007 | Yes | Yes | Yes |

The goal is that the **same water and trail logic** runs on all six combinations. Only the sensor interface and board pin definitions should differ.

---

# How the whole system fits together

A normal remote environmental station can look like this:

```text
FIELD SITE

Sensor
  │
  │ I2C / UART / BLE
  ▼
Seeed or RAK field node
  │
  │ Meshtastic / LoRa
  ▼
Mesh relay(s)
  │
  ▼
Internet-connected gateway
  │
  │ HTTPS
  ▼
Cloud ingest / database / dashboard
```

For the current HOBO network, the practical path is conceptually:

```text
HOBO logger
   ↓ BLE
field Seeed or RAK
   ↓ Meshtastic
mesh
   ↓
Heltec Home gateway
   ↓ HTTPS
Vercel ingest
   ↓
Neon database
   ↓
web dashboard
```

The distance-sensor nodes are intended to use the same general network architecture.

---

# Why self-recovery matters

A field radio can be physically difficult to reach. It might be:

- on a mountain;
- along a creek;
- several hours down a dirt road;
- mounted high on a tree;
- inside a weatherproof enclosure;
- deployed during a season when access is difficult.

If a node locks up, the cost is not just a reboot. The cost may be a full field trip.

The recovery system therefore follows a simple philosophy:

> **Try the least destructive fix first. Preserve configuration. Escalate only when the node is clearly unhealthy.**

The firmware should never erase settings just because BLE failed or a sensor disappeared.

---

# Watchdog recovery

A **watchdog** is a hardware timer inside the nRF52840.

Imagine a teacher asking a student to say “I am still working” every few minutes. If the student stops answering for too long, the teacher restarts the activity.

The watchdog works the same way.

The self-recovery supervisor periodically feeds the watchdog. If the firmware becomes completely stuck and stops feeding it, the nRF52840 resets itself.

Current self-recovery behavior uses an approximately **15-minute watchdog timeout**.

The long timeout is intentional. A remote node can legitimately spend time sleeping or doing radio work. The watchdog is meant to catch a real lockup, not punish normal low-power behavior.

The watchdog is configured to continue running while the CPU sleeps.

If another part of the firmware already owns the watchdog, the self-recovery module does not blindly take it over. It checks first.

---

# Solar and low-battery recovery

Solar-powered radios have a special failure mode that does not show up when testing on USB.

## The problem

A radio can drain its battery low enough to enter a deep power-off state.

Then the sun comes up.

The solar panel charges the battery again.

But if the microcontroller has no valid battery-rise wake source, the battery can become healthy while the processor remains asleep.

From the outside the system can appear confusing:

```text
battery has power
charger light is on
radio does not answer
USB cable is inserted
radio suddenly wakes
```

That is the exact type of failure the recovery baseline is designed to avoid.

## Seeed recovery behavior

The Seeed XIAO battery is measured through its battery divider on nRF52840 analog input AIN7.

The field baseline adds an nRF52 low-power comparator wake source on that battery input.

In plain language:

```text
battery becomes too low
        ↓
radio safely enters deep SYSTEM OFF
        ↓
solar panel keeps charging battery
        ↓
battery voltage rises to recovery threshold
        ↓
low-power comparator notices the rise
        ↓
nRF52840 wakes automatically
        ↓
normal boot resumes
```

The current Seeed configuration treats about **3.40 V as the low end of the battery curve** instead of allowing the radio to ride the cell down toward a much more dangerous brownout region.

The comparator configuration corresponds to a battery-rise wake point of roughly **3.7 V**, depending on real regulator and divider conditions.

The exact number should be treated as an engineering threshold, not a laboratory-grade battery measurement.

## RAK recovery behavior

The RAK4631 board support already includes a battery low-power comparator input.

The canonical baseline retains that behavior and also uses the safer low-voltage battery curve with about **3.40 V as the bottom of the configured OCV range**.

The RAK comparator configuration is intended to wake after the battery has recovered substantially, rather than repeatedly bouncing on and off at the low-voltage edge.

## Why the wake threshold is above the shutoff threshold

This difference is called **hysteresis**.

If the node shut off at 3.40 V and woke again at 3.41 V, a weak solar panel could cause this:

```text
3.40 V → off
3.41 V → on
3.40 V → off
3.41 V → on
3.40 V → off
```

That wastes energy and can create repeated boot loops.

A higher recovery threshold gives the battery time to recharge before the node tries to operate again.

---

# BLE recovery

BLE means **Bluetooth Low Energy**.

The HOBO loggers use BLE, and future devices may also use BLE.

BLE can occasionally get into a stale state. The recovery code therefore tracks whether the radio appears to be connected or still searching.

## Healthy connection

A healthy active logger connection is left alone.

The recovery supervisor should **not periodically tear down a good BLE connection** just because a timer expired.

## Disconnected scanning

While disconnected, the recovery system reduces scanner duty cycle to save power.

The current recovery target is approximately:

```text
scan interval: 200 ms
scan window:    20 ms
receive duty:   about 10%
active scan:    off / passive
```

That means the receiver is not listening at full power continuously while a logger is unavailable.

## Scanner refresh

If the node remains disconnected, the scanner can be refreshed periodically.

That is a much smaller intervention than rebooting the entire radio.

## Long-disconnect reboot

If the node has failed to establish its expected BLE relationship for a very long period, the recovery supervisor can reboot the microcontroller to rebuild the BLE stack from a clean state.

The current long-disconnect threshold is approximately **6 hours**.

That is deliberately conservative. A temporary missing logger should not cause constant reboots.

---

# What recovery is allowed to change

Recovery actions are meant to restore operation, not wipe the station.

A normal `RECOVER`, watchdog reset, battery cycle, or ordinary firmware update should preserve:

- Meshtastic node identity;
- Meshtastic channels;
- channel PSKs/keys;
- NodeDB;
- radio configuration;
- saved logger lock;
- saved sensor configuration;
- distance calibration;
- trail counters;
- other persistent field state unless a future command explicitly says otherwise.

The recovery system must **not use a factory-reset approach as routine maintenance**.

A remote sensor that comes back online with a new identity and erased calibration is not truly recovered.

---

# Current HOBO support

The recovery baseline grew out of the project's HOBO work.

The universal HOBO reader supports:

| Logger | Main use | Automatic telemetry | Manual `READ` |
|---|---|---|---|
| MX2001 | water level + temperature | Yes | Yes |
| MX2201 | temperature | Yes | Yes |
| MX2203 | temperature | Yes | Yes |

## How automatic HOBO telemetry works

The radio does **not** simply start its own timer and assume the HOBO has logged a new value.

Instead, the radio asks the logger for status information and follows the logger's write pointer.

Simplified sequence:

```text
connect to logger
      ↓
read STATUS
      ↓
learn logging interval and write pointer
      ↓
wait
      ↓
write pointer advances
      ↓
a real new logger record exists
      ↓
request NEWREAD64
      ↓
decode record
      ↓
queue Meshtastic packet
      ↓
mark pointer consumed only after queue succeeds
```

This is scientifically important because the radio is following **actual logged data**, not inventing a schedule.

A manual `READ` is intentionally separate from the automatic write-pointer path. Manually checking the sensor should not accidentally consume the next automatic record.

## HOBO lock

A field node can remember the intended logger.

Typical field workflow:

```text
LOGGER
READ
LOCK
```

Once locked, the node can persist the intended logger assignment across reboot.

---

# Distance-sensor project

The next major subsystem is the shared distance-sensor module.

Its home is:

```text
src/modules/Telemetry/DistanceSensor/
```

The important architectural decision is that **sensor hardware and field behavior are separate**.

```text
                     ┌──────────────────────────┐
                     │ DistanceSensorModule     │
                     │ shared field behavior    │
                     └────────────┬─────────────┘
                                  │
                  ┌───────────────┴───────────────┐
                  │                               │
                  ▼                               ▼
          ┌───────────────┐               ┌───────────────┐
          │ MODE WATER    │               │ MODE TRAIL    │
          │ stage logic   │               │ counter logic │
          └───────┬───────┘               └───────┬───────┘
                  │                               │
                  └───────────────┬───────────────┘
                                  │
                                  ▼
                     ┌──────────────────────────┐
                     │ common distance reading  │
                     └────────────┬─────────────┘
                                  │
                 ┌────────────────┼────────────────┐
                 ▼                ▼                ▼
             SEN0590       SEN0311/A02YYUW   SEN0313/A01NYUB
```

That means replacing a SEN0311 with a SEN0313 should not require rewriting:

- water-stage math;
- trail-count logic;
- calibration storage;
- watchdog behavior;
- battery reporting;
- Meshtastic messaging;
- recovery logic.

Only the hardware driver should care how that particular sensor returns a distance.

---

# Water-level mode

## The problem

Suppose an ultrasonic sensor is mounted above a creek.

The sensor measures the distance from itself down to the water surface.

That number is **not automatically the water depth or stage**.

For example:

```text
sensor
  │
  │ 6.83 ft measured distance
  ▼
water surface
```

The firmware needs a known reference before it can convert that distance into stage.

## Field calibration concept

The node will be mounted first.

Then someone measures the actual stage using a tape, staff gauge, or other field reference.

Suppose:

```text
sensor raw distance = 6.83 ft
known water stage   = 1.42 ft
```

The field user sends:

```text
CAL STAGE 1.42FT
```

The firmware can then calculate:

```text
reference = raw distance + known stage
reference = 6.83 + 1.42
reference = 8.25 ft
```

That reference is stored persistently.

Later, if the water rises and the sensor now reads 5.83 ft:

```text
stage = saved reference - raw distance
stage = 8.25 - 5.83
stage = 2.42 ft
```

The node did not need to know its mounting height before installation.

## Why this is useful

A crew can choose the safest practical mounting point in the field.

The mounting location does not have to match a preprogrammed number.

After installation, one known stage measurement teaches the node its geometry.

## Raw distance must still be saved

The firmware should never throw away the original sensor reading and keep only calculated stage.

A scientifically useful record should retain both:

```text
raw distance: 5.83 ft
stage:        2.42 ft
reference:    8.25 ft
```

If calibration is later questioned, the raw distance is still available.

## Future rating-curve use

Stage and discharge are not the same thing.

A creek's discharge relationship may later be represented by a rating curve.

The preferred architecture is to keep raw distance and stage authoritative on the field node, then calculate discharge in the database/dashboard layer when practical.

That allows a rating curve to be improved later without rewriting old raw measurements.

---

# Trail-counter mode

A trail counter uses distance differently.

Instead of looking down at water, the sensor looks across a trail toward the opposite side.

Example:

```text
sensor  --------------------------------  opposite side
                 clear trail

clear-path sensor reading = 5.18 ft
```

The exact trail width may not be known until the sensor is physically mounted.

That is why the preferred field command is:

```text
CAL CLEAR
```

## What `CAL CLEAR` should do

The trail must be empty.

The firmware samples the clear path multiple times and calculates a stable baseline.

For example:

```text
5.17 ft
5.18 ft
5.18 ft
5.19 ft
5.17 ft
```

A representative baseline might become:

```text
baseline = 5.18 ft
```

That stored sensor baseline is better than blindly using a tape-measure width because it reflects what the sensor itself actually sees.

## Why the trigger should not simply be “anything under 5.18 ft”

Real sensors have noise.

An empty trail could naturally bounce between values such as:

```text
5.14 ft
5.20 ft
5.16 ft
5.19 ft
```

If every value below 5.18 ft were counted, the radio could count imaginary hikers.

Instead, the design uses a **trigger delta**.

Example:

```text
baseline = 5.18 ft
trigger  = 12 in closer than baseline
```

A person is not considered present until the reading becomes roughly:

```text
less than 4.18 ft
```

## State-machine counting

The counter should work like this:

```text
CLEAR
  │
  │ object enters beam
  ▼
BLOCKED
  │
  │ increment count once
  ▼
WAIT FOR CLEAR
  │
  │ beam becomes clear again
  ▼
CLEAR / ARMED AGAIN
```

A person standing in the beam for five seconds still counts as **one** person.

## Fast rearm

The previous PIR experiments showed why long fixed hold times are undesirable for groups.

The distance-counter design should rearm based on the beam actually clearing, not an arbitrary 10- or 11-second delay.

The target behavior is a rearm on the order of a few hundred milliseconds when the sensor and filtering allow it.

That gives the system a chance to count two people walking close together.

## Hysteresis

Trail detection should use different thresholds for becoming blocked and becoming clear.

This prevents the state from rapidly flipping back and forth near one noisy threshold.

Example concept:

```text
baseline:       5.18 ft
blocked below:  4.18 ft
clear above:    4.68 ft
```

The exact defaults will be finalized during bench testing.

## Obstruction detection

A branch, rock, snowbank, animal, or vandalized mounting position could permanently block the beam.

The node should detect an unusually long blocked state and report something like:

```text
OBSTRUCTION
```

It should **not silently recalibrate itself around the obstruction**.

Automatic silent recalibration could make the sensor appear healthy while actually counting incorrectly.

---

# Supported distance sensors

The first firmware generation is designed around the sensors already physically available for testing.

## DFRobot SEN0590

Project role:

- I2C distance sensor;
- strong candidate for trail-counter testing;
- useful where fast repeated distance measurements are needed.

The driver should present a normal distance value to the shared module. The water/trail code should not care that the source is I2C.

## DFRobot SEN0311 / A02YYUW

Project role:

- UART distance sensor;
- short/medium-range ultrasonic testing;
- useful for trail testing and shorter water installations.

The shared module receives a parsed distance from a UART driver.

## DFRobot SEN0313 / A01NYUB

Project role:

- UART distance sensor;
- longer-range ultrasonic option;
- primary first candidate for creek stage installations approaching the project's roughly 25-foot target.

The SEN0311 and SEN0313 families are similar enough that much of the UART parsing layer can be shared, while model-specific range and validation rules remain separate.

---

# Planned direct-message commands

The exact command grammar will be finalized during implementation, but the design target is intentionally simple enough to use from the Meshtastic phone app in the field.

## General commands

```text
HELP
STATUS
HEALTH
VERSION
UPTIME
POWER
BATTERY
SENSOR
RAW
READ
STATS
REBOOT
RECOVER
```

## Mode commands

```text
MODE
MODE WATER
MODE TRAIL
```

## Water commands

```text
CAL
CAL STAGE 1.42FT
CAL STAGE 17IN
CAL RESET
STAGE
RAW
INTERVAL 15MIN
```

## Trail commands

```text
CAL CLEAR
BASELINE
TRIGGER 12IN
COUNT
COUNT TODAY
COUNT TOTAL
RESET COUNT
```

## Example water installation conversation

```text
User → node: MODE WATER
Node → user: MODE: WATER

User → node: RAW
Node → user: RAW DISTANCE: 6.83 ft

User → node: CAL STAGE 1.42FT
Node → user: CAL SAVED
             Raw: 6.83 ft
             Known stage: 1.42 ft
             Reference: 8.25 ft

User → node: READ
Node → user: Raw: 6.82 ft
             Stage: 1.43 ft
             Sensor: OK
```

## Example trail installation conversation

```text
User → node: MODE TRAIL
Node → user: MODE: TRAIL

User → node: CAL CLEAR
Node → user: Keep trail clear. Sampling...
             Baseline: 5.18 ft
             Noise: ±0.04 ft
             Calibration saved.

User → node: TRIGGER 12IN
Node → user: Trigger delta saved: 12 in

User → node: COUNT
Node → user: Today: 37
             Lifetime: 1,842
```

Configuration-changing commands should eventually be restricted to appropriate direct/admin traffic rather than accepted blindly from any packet on the mesh.

---

# Telemetry and data-integrity rules

This project treats sensor data as scientific/operational data, not just a demo value on a screen.

## Rule 1: preserve raw measurements

If a sensor says 1842 mm, store that raw value even if the firmware also calculates stage.

## Rule 2: derived values should be reproducible

A derived stage should be traceable to:

```text
raw distance
saved calibration reference
calculation method
```

## Rule 3: flag bad data instead of secretly replacing it

Possible QA/QC states may include:

```text
OK
NO_ECHO
OUT_OF_RANGE
NOISY
OBSTRUCTION
SENSOR_TIMEOUT
CALIBRATION_MISSING
```

A questionable measurement should be marked questionable.

The firmware should not silently invent a believable number.

## Rule 4: do not flood LoRa with high-rate samples

A trail sensor may sample many times per second so it can recognize a person.

Those internal samples do **not** all belong on the mesh.

The radio should use high-rate local sampling for detection but send compact event or summary telemetry.

Example trail telemetry:

```text
count_today
count_total
last_event_time
baseline_distance
minimum_event_distance
sensor_health
battery
```

## Rule 5: water telemetry should preserve both raw and derived information

Example water telemetry:

```text
raw_distance_mm
stage_mm
calibration_reference_mm
sensor_model
sensor_health
battery_voltage
battery_percent
timestamp
```

## Rule 6: configuration must persist

The following should survive a reboot and normal firmware update:

```text
selected mode
selected sensor type
water calibration reference
trail baseline
trail trigger settings
persistent counters
other field configuration
```

---

# Branch map

The repository has accumulated experimental and production branches over time. The current architecture gives them clear roles.

| Branch | Role | New development should start here? |
|---|---|---|
| `field-self-recovery` | **Default canonical field-node baseline** | **Yes** |
| `distance-self-recovery-seeed` | Seeed distance-sensor development | For Seeed distance work |
| `distance-self-recovery-rak4631` | RAK distance-sensor development | For RAK distance work |
| `hobo-self-recovery-seeed` | validated Seeed HOBO recovery/solar reference | No; reference/deployment line |
| `hobo-self-recovery-rak4631` | validated RAK HOBO recovery/solar reference | No; reference/deployment line |
| `heltec-gateway-clean-rebuild` | Heltec V4 internet/cloud gateway | Gateway work only |
| older experiment branches | historical tests | No |

The purpose of `field-self-recovery` is to stop future sensor projects from repeatedly branching from old experimental code.

---

# Source-code layout

High-level structure:

```text
firmware/
│
├── README.md
│   └── this front-page guide
│
├── FIELD_SELF_RECOVERY.md
│   └── detailed baseline/recovery reference
│
├── docs/
│   └── longer design and historical documentation
│
├── src/
│   └── modules/
│       └── Telemetry/
│           │
│           ├── HOBOMX2001MX2201MX2203/
│           │   └── universal HOBO reader
│           │
│           ├── HOBOSelfRecovery/
│           │   ├── recovery supervisor
│           │   └── remote diagnostics
│           │
│           └── DistanceSensor/
│               └── shared distance project home
│
├── variants/
│   └── nrf52840/
│       ├── seeed_xiao_nrf52840_kit/
│       │   └── Seeed power / battery / board settings
│       └── rak4631/
│           └── RAK power / battery / board settings
│
└── .github/
    └── workflows/
        └── field recovery CI builds
```

## Planned DistanceSensor structure

The target internal layout is approximately:

```text
src/modules/Telemetry/DistanceSensor/
│
├── DistanceSensorModule.h
├── DistanceSensorModule.cpp
│
├── DistanceConfig.h
├── DistanceCalibration.cpp
│
├── WaterStage.cpp
├── TrailCounter.cpp
│
└── drivers/
    ├── DistanceSensorDriver.h
    ├── SEN0590.h
    ├── SEN0590.cpp
    ├── A02YYUW.h
    ├── A02YYUW.cpp
    ├── A01NYUB.h
    └── A01NYUB.cpp
```

The exact filenames may change during implementation, but the separation of responsibilities should remain.

---

# Build and CI rules

CI means **continuous integration**.

Every change to the canonical recovery branch should prove that both supported board families still compile.

The canonical workflow builds:

```text
seeed_xiao_nrf52840_kit
rak4631
```

A common recovery change is not considered safe merely because one board compiles.

Why?

Because the project intentionally shares code between Seeed and RAK.

A change that accidentally fixes Seeed while breaking RAK is still a broken change.

The final canonical baseline was verified by a dual-board GitHub Actions run with both targets passing.

---

# Safe firmware-update rules

This section is intentionally repetitive because it protects deployed nodes.

## Normal update rule

**Do not erase flash before an ordinary update.**

## Do not use factory images for ordinary upgrades

A factory image can be appropriate for a truly corrupted device that is being intentionally rebuilt from scratch.

It is not the normal upgrade path for a configured field node.

## Preserve NVS

NVS is the non-volatile storage area containing important persistent settings.

Ordinary field updates should preserve it.

Important settings include:

```text
node identity
channels
keys / PSKs
NodeDB
radio settings
Wi-Fi settings where applicable
sensor configuration
logger lock
calibration
counters
```

## Preferred update philosophy

Use the least destructive path available:

```text
normal OTA / normal application flash
        ↓
preserve persistent settings
        ↓
reboot
        ↓
verify node identity and configuration
```

Never make `erase-flash` a routine troubleshooting step for production nodes.

---

# Field deployment workflow

A good field deployment should prove more than “the LED turned on.”

## Before leaving the shop

Confirm:

1. correct board build;
2. correct antenna attached before transmitting;
3. battery connected correctly;
4. solar charging works;
5. Meshtastic identity/settings are correct;
6. remote DM works;
7. battery telemetry works;
8. sensor can be read;
9. recovery commands respond;
10. configuration survives reboot.

## At the site

For a water node:

```text
mount sensor
verify clear view of water
check RAW distance
measure known stage
send CAL STAGE <value>
read again
compare stage with field measurement
reboot
verify calibration survived
confirm telemetry path
```

For a trail node:

```text
mount sensor
make sure trail is empty
check RAW distance
send CAL CLEAR
walk through beam several times
verify one count per person/pass
verify fast rearm
stand in beam and verify only one count
clear beam
verify node rearms
reboot
verify baseline and counters survived
```

## Before walking away

The station should pass an end-to-end test:

```text
sensor
  ↓
field radio
  ↓
Meshtastic packet
  ↓
expected relay/gateway
  ↓
expected destination/dashboard
```

A successful local sensor read alone does not prove the remote system works.

---

# Troubleshooting philosophy

Always start with the least invasive question.

A recommended escalation pattern is:

```text
1. Is the node answering PING/STATUS?
2. Is battery voltage healthy?
3. Is the sensor answering RAW/READ?
4. Is the expected BLE/UART/I2C interface healthy?
5. Can a local scanner/interface restart fix it?
6. Can RECOVER safely reboot it?
7. Did settings survive?
8. Only then consider physical access or destructive recovery.
```

This hierarchy exists because every remote field trip costs time.

The software should provide enough information to distinguish:

- radio problem;
- battery problem;
- sensor problem;
- BLE problem;
- calibration problem;
- obstruction problem;
- network-path problem;
- cloud-ingest problem.

A single message saying only `ERROR` is not enough for a remote scientific station.

---

# Glossary

## BLE

**Bluetooth Low Energy.** The short-range radio protocol used to communicate with devices such as HOBO loggers.

## LoRa

A long-range, low-data-rate radio technology. Meshtastic uses LoRa to move packets between nodes over long distances.

## Meshtastic

Open-source firmware and protocol that allows LoRa radios to form a mesh network.

## nRF52840

The microcontroller used by both the Seeed XIAO nRF52840 and the RAK4631. It runs the firmware and contains features such as BLE, watchdog hardware, analog inputs, and low-power comparators.

## SX1262

The LoRa radio chip used by these nodes.

## RAK4631

A WisBlock core module containing the nRF52840 and SX1262.

## RAK19007

The WisBlock baseboard standardized for new distance-sensor deployments in this project.

## UART

A simple serial communication method using transmit and receive wires. The SEN0311 and SEN0313 distance sensors use UART.

## I2C

A two-wire digital bus commonly used for sensors. The SEN0590 is planned to use the I2C driver path.

## NVS

**Non-Volatile Storage.** Memory that keeps important configuration after power is removed.

## Watchdog

A hardware timer that resets the microcontroller if the firmware stops proving it is alive.

## SYSTEM OFF

A very low-power nRF52 state. The processor is essentially shut down until an approved wake source occurs.

## LPCOMP

The nRF52 **low-power comparator**. It can watch an analog voltage while using very little energy and can wake the chip when the voltage crosses a configured threshold.

## OCV

**Open-Circuit Voltage.** In this repository the battery OCV table helps map battery voltage to battery percentage and defines the low end of the battery curve.

## Stage

The height of water relative to a chosen reference. Stage is not automatically the same as water depth or discharge.

## Discharge

The volume of water passing a point per unit time, commonly expressed in cubic feet per second (cfs). Discharge may later be estimated from stage using a rating curve.

## Baseline

For a trail sensor, the normal clear-path distance when no person is blocking the beam.

## Hysteresis

Using one threshold to enter a state and a different threshold to leave it. This prevents noisy measurements from rapidly switching between states.

## QA/QC

**Quality Assurance / Quality Control.** Rules for identifying questionable measurements, sensor failures, or conditions that could make data unreliable.

## CI

**Continuous Integration.** Automatic cloud builds that compile the firmware after changes so problems are caught before field deployment.

## OTA

**Over the Air.** Updating firmware without connecting a programming cable directly to the device.

---

# Current development status

## Canonical baseline

```text
field-self-recovery
```

This is now the repository default branch and the required starting point for new unattended nRF52 field-sensor development.

## Validated baseline hardware builds

```text
Seeed XIAO nRF52840 + Wio-SX1262: PASS
RAK4631:                               PASS
```

## Distance development branches

```text
distance-self-recovery-seeed
distance-self-recovery-rak4631
```

## Distance module home

```text
src/modules/Telemetry/DistanceSensor/
```

## First supported distance sensors

```text
DFRobot SEN0590
DFRobot SEN0311 / A02YYUW
DFRobot SEN0313 / A01NYUB
```

## First application modes

```text
MODE WATER
MODE TRAIL
```

## Core project rule going forward

**Do not build a new remote field sensor from an old experimental branch unless there is a specific technical reason. Start from `field-self-recovery`, inherit the power/recovery protections, and add only the sensor/application layer that is actually new.**

For the detailed recovery design, see [`FIELD_SELF_RECOVERY.md`](FIELD_SELF_RECOVERY.md).

For the distance-sensor design, see [`src/modules/Telemetry/DistanceSensor/README.md`](src/modules/Telemetry/DistanceSensor/README.md).
