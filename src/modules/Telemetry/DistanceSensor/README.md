# Distance Sensor Firmware

This folder contains the first working implementation of the **field-configurable distance-sensor firmware** for the Meshtastic field network.

It is built on the `field-self-recovery` foundation, so the distance code does not replace the recovery/solar work. It inherits it.

Supported field platforms are:

```text
Seeed XIAO nRF52840 + Wio-SX1262
RAK4631 + RAK19007
```

The same application supports three DFRobot sensors:

```text
DFRobot SEN0590
DFRobot SEN0311 / A02YYUW
DFRobot SEN0313 / A01NYUB
```

And the same firmware can be configured in the field for either:

```text
MODE WATER
MODE TRAIL
```

The important design idea is that **the sensor driver and the field application are separate**. A distance is read first. Water or trail logic decides what that distance means afterward.

---

# 1. Current status

## Implemented now

The distance branches now compile the real distance subsystem rather than the old HOBO module.

Implemented features include:

- common `DistanceSensorDriver` interface;
- SEN0590 I2C driver;
- common DFRobot UART ultrasonic parser for SEN0311/A02YYUW and SEN0313/A01NYUB;
- sensor selection by Meshtastic direct message;
- limited automatic sensor detection;
- `MODE WATER`;
- `MODE TRAIL`;
- field water calibration with `CAL STAGE`;
- field trail calibration with `CAL CLEAR`;
- saved calibration across reboot/power loss;
- configurable trail trigger and clear hysteresis;
- two-sample blocked confirmation;
- two-sample clear/rearm confirmation;
- obstruction detection after a continuously blocked beam;
- daily and lifetime trail counters;
- calendar-day rollover when the node has valid time;
- persistent count checkpoints;
- raw-distance readback;
- configurable reporting interval;
- compact versioned private Meshtastic telemetry packets;
- battery/device telemetry inherited from Meshtastic;
- a distance-specific 15-minute watchdog supervisor;
- non-destructive `RECOVER` / `REBOOT`;
- inherited solar low-voltage protection and solar-recharge wake behavior;
- separate CI builds for Seeed and RAK4631.

## Build status

Both initial implementation branches compiled successfully in GitHub Actions:

```text
distance-self-recovery-seeed      PASS
distance-self-recovery-rak4631    PASS
```

Compilation proves that the code integrates with the Meshtastic firmware tree for both boards.

It does **not** replace bench testing. Each physical sensor still needs to be connected and tested before a field deployment is considered validated.

---

# 2. Branches

```text
field-self-recovery
    ├── distance-self-recovery-seeed
    └── distance-self-recovery-rak4631
```

`field-self-recovery` remains the canonical recovery/solar foundation.

The distance branches add the actual distance module and board-specific build flags.

They intentionally disable the normal GPS module and generic serial module because the distance firmware reserves the hardware UART for the ultrasonic sensor.

---

# 3. Supported sensors

| Sensor | Firmware name | Interface | Nominal role in this project |
|---|---|---|---|
| DFRobot SEN0590 | `SEN0590` | I2C | fast close/medium-range distance and trail testing |
| DFRobot SEN0311 / A02YYUW | `SEN0311` | UART 9600 | trail counting and shorter water installations |
| DFRobot SEN0313 / A01NYUB | `SEN0313` | UART 9600 | longer water/stage installations, up to roughly the project's 25 ft target |

The UART sensors use the same basic 4-byte frame format, so they share one parser. Model selection changes the allowed range and the model label rather than duplicating the whole UART implementation.

---

# 4. Driver architecture

The application sees this conceptual interface:

```text
begin sensor
read sensor
    ↓
DistanceReading
    ├── status
    ├── distance_mm
    └── measurement time
```

A valid driver result looks conceptually like:

```text
status: OK
distance_mm: 1842
```

An invalid result may look like:

```text
status: TIMEOUT
```

or:

```text
status: CHECKSUM
```

or:

```text
status: OUT_OF_RANGE
```

The water and trail state machines never need to decode I2C or UART themselves.

---

# 5. SEN0590 implementation

The SEN0590 driver uses I2C address:

```text
0x74
```

The implemented measurement transaction is:

```text
write 0xB0 to register 0x10
wait about 50 ms
select register 0x02
read two bytes
combine them as a big-endian distance value
apply the manufacturer's +10 mm compensation
```

The driver rejects values outside its configured usable range rather than treating them as real measurements.

---

# 6. SEN0311 / SEN0313 UART implementation

The two ultrasonic UART sensors use 9600 baud and a four-byte frame:

```text
Byte 0: 0xFF
Byte 1: distance high byte
Byte 2: distance low byte
Byte 3: checksum
```

The checksum must equal:

```text
(Byte0 + Byte1 + Byte2) & 0xFF
```

The distance is reconstructed as:

```text
distance_mm = (Byte1 << 8) | Byte2
```

The parser does not trust random serial bytes. It:

1. searches for the `0xFF` frame header;
2. collects the remaining bytes;
3. verifies the checksum;
4. applies the selected model's valid range;
5. returns a normalized `DistanceReading`.

This allows the parser to recover if it begins listening halfway through a serial frame.

---

# 7. Sensor selection

The configured sensor type is saved persistently.

Commands:

```text
SENSOR
SENSOR AUTO
SENSOR SEN0590
SENSOR SEN0311
SENSOR A02YYUW
SENSOR SEN0313
SENSOR A01NYUB
```

`SENSOR` reports both the saved configuration and the currently active driver.

## AUTO behavior

`SENSOR AUTO` currently works like this:

```text
probe I2C address 0x74
    ↓
if present → use SEN0590

otherwise
    ↓
listen for a valid DFRobot UART frame
    ↓
if found → use generic compatible UART ultrasonic driver
```

The firmware cannot reliably distinguish SEN0311 from SEN0313 from the shared UART frame alone. If the exact model matters for range validation, set it explicitly with `SENSOR SEN0311` or `SENSOR SEN0313`.

---

# 8. Board wiring interfaces

## Seeed XIAO nRF52840 + Wio-SX1262

The Wio-SX1262 already uses several of the normal XIAO pins. The distance branch therefore reserves interfaces that do not collide with the LoRa radio.

### UART ultrasonic sensors

```text
Sensor TX  → XIAO D7 / hardware UART RX
Sensor RX  → XIAO D6 / hardware UART TX if a future command mode needs it
GND        → GND
Power      → appropriate sensor supply
```

Normal GPS support is disabled in this branch so GPS cannot fight the distance sensor for D6/D7.

### SEN0590 I2C

The default XIAO Meshtastic variant routes `Wire` to the NFC pads:

```text
SDA → D30 / NFC1
SCL → D31 / NFC2
```

Those are intentionally used instead of D4/D5 because D4/D5 are part of the Wio-SX1262 radio wiring on the standard kit.

The build already enables the NFC pins as GPIOs.

## RAK4631 + RAK19007

### UART ultrasonic sensors

The firmware uses the RAK4631 hardware UART1 exposed through the WisBlock base:

```text
RX1 = Arduino pin 15
TX1 = Arduino pin 16
```

The exact RAK19007 connector pins used in the physical wiring should be checked against the RAK19007 silkscreen/datasheet before soldering a field unit.

### SEN0590 I2C

The firmware uses the normal WisBlock I2C bus:

```text
SDA = WB_I2C1_SDA / Arduino pin 13
SCL = WB_I2C1_SCL / Arduino pin 14
```

The distance branch is standardized on **RAK19007** for new builds.

---

# 9. Power note

The firmware controls the communications interface; it does not magically make every supply voltage safe.

Before bench wiring, verify the sensor's actual supply requirements and the logic level presented to the nRF52840.

The nRF52840 GPIO logic is 3.3 V logic. Do not put an unsafe 5 V logic signal directly into a GPIO just because a sensor can itself be powered from 5 V.

---

# 10. Operating modes

Commands:

```text
MODE WATER
MODE TRAIL
MODE IDLE
```

The selected mode is saved in:

```text
/prefs/distance_sensor.bin
```

It survives ordinary reboot, watchdog reset, manual recovery, and battery loss.

`MODE IDLE` leaves the distance module configured but stops automatic water/trail sampling.

---

# 11. Water mode

Water mode treats the sensor as an overhead distance measurement.

The raw quantity is:

```text
D = sensor-to-water distance
```

The firmware does not assume that this is stage.

## Calibration

After the sensor is physically mounted, measure the real stage/depth at that moment and send it to the node.

Example:

```text
MODE WATER
CAL STAGE 1.42FT
```

The firmware takes several valid sensor measurements and uses the median raw distance.

Suppose:

```text
median raw distance = 6.83 ft
known stage         = 1.42 ft
```

It calculates and saves:

```text
reference = raw + known stage
reference = 8.25 ft
```

Future stage is:

```text
stage = saved reference - current raw distance
```

If the water rises one foot, sensor-to-water distance falls one foot and calculated stage rises one foot.

## Why this is useful

The sensor can be mounted wherever the site actually allows:

- bridge beam;
- tree;
- post;
- rock anchor;
- other stable overhead structure.

The final mounting height does not have to be known when the firmware is compiled.

## Supported calibration units

Examples:

```text
CAL STAGE 1.42FT
CAL STAGE 17IN
CAL STAGE 43CM
CAL STAGE 430MM
CAL STAGE 0.43M
```

Internally the firmware stores millimeters.

## Read commands

```text
RAW
READ
```

`RAW` reports only the measured sensor distance.

`READ` in water mode reports raw distance and calculated stage if calibration exists.

If calibration is missing, the firmware reports the raw measurement and explicitly says stage is not calibrated. It does not invent a stage value.

---

# 12. Trail mode

Trail mode samples distance locally and turns a blocked beam into a single pass event.

Example geometry:

```text
sensor  ------------------------------ opposite side
                 trail
```

The normal empty-trail distance is learned after installation.

## Clear-path calibration

With nobody in the beam:

```text
MODE TRAIL
CAL CLEAR
```

The firmware collects multiple valid readings, sorts them, and stores the median as the baseline.

Example:

```text
1575 mm
1578 mm
1577 mm
1576 mm
1579 mm

saved baseline = 1577 mm
```

That is more useful than trusting only a tape measurement because it captures the actual sensor angle and target surface.

---

# 13. Trail trigger and hysteresis

Defaults are approximately:

```text
TRIGGER = 12 in closer than baseline
CLEAR   =  6 in closer than baseline
```

Commands:

```text
TRIGGER 12IN
CLEAR 6IN
```

The trigger value must be larger than the clear value.

Example with a 1577 mm baseline:

```text
trigger delta = 305 mm
blocked below = 1272 mm

clear delta   = 152 mm
clear above   = 1425 mm
```

That gap is hysteresis. It prevents one noisy person/object near the threshold from rapidly switching between blocked and clear.

---

# 14. Trail counting state machine

The implemented logic is:

```text
CLEAR
  │
  │ two consecutive measurements below block threshold
  ▼
BLOCKED
  │
  ├── increment daily count once
  ├── increment lifetime count once
  └── remember minimum distance during event
  │
  │ two consecutive measurements above clear threshold
  ▼
CLEAR / ARMED AGAIN
```

The important behavior is:

```text
one person blocks beam for 0.3 s → one count
one person blocks beam for 5 s   → one count
```

It does not count every sample while the beam is blocked.

The normal trail sampling loop runs at approximately 100 ms between module iterations. Actual effective rate also depends on the physical sensor's response time.

---

# 15. Obstruction detection

If the node remains in the blocked state for about 30 seconds, it flags:

```text
OBSTRUCTION
```

Possible causes include:

- branch;
- snow;
- vegetation;
- animal;
- debris;
- sensor knocked out of alignment.

The firmware does **not** silently relearn that obstruction as the new baseline.

Recalibration requires an explicit operator command.

---

# 16. Trail counters

Command:

```text
COUNT
```

Reports:

```text
Daily
Lifetime
CLEAR/BLOCKED state
Obstruction state
Clock synchronization state
```

Reset only the daily count:

```text
RESET DAILY
```

Reset both daily and lifetime counts:

```text
RESET COUNT
```

The firmware uses Meshtastic's RTC time when valid. A UTC day number is saved with the count. When a new valid UTC day is detected, `dailyCount` resets automatically while `lifetimeCount` continues.

If the node does not yet have valid time, it reports the clock as unsynchronized rather than pretending that a calendar rollover is known.

## Flash-write compromise

The live count increments in RAM immediately.

To reduce unnecessary flash wear, count state is checkpointed after several events and again at periodic telemetry reports. This means a sudden hard power loss immediately after a few new events could theoretically lose a small number of not-yet-checkpointed counts. That tradeoff can be changed later after field traffic volume is known.

---

# 17. Reporting interval

Command examples:

```text
INTERVAL 30SEC
INTERVAL 5MIN
INTERVAL 15MIN
INTERVAL 1HR
```

Allowed range in the first implementation is approximately 5 seconds through 24 hours.

In water mode, the interval controls automatic measurement/report timing.

In trail mode, local detection continues rapidly, but compact summary telemetry is sent only at the reporting interval.

This prevents high-rate trail sampling from flooding the LoRa mesh.

---

# 18. Automatic telemetry packet

Distance telemetry uses Meshtastic `PRIVATE_APP` with a compact versioned binary packet.

Current packet length:

```text
24 bytes
```

Layout:

| Byte(s) | Meaning |
|---|---|
| 0–1 | ASCII `DS` magic |
| 2 | packet version, currently `1` |
| 3 | mode: idle/water/trail |
| 4 | active sensor type |
| 5 | flags |
| 6–7 | little-endian sequence number |
| 8–11 | raw distance in mm, or zero if invalid |
| 12–15 | water: signed stage mm; trail: daily count |
| 16–19 | water: signed calibration reference mm; trail: lifetime count |
| 20–23 | Unix time when valid, otherwise zero |

Flag bits currently represent:

```text
bit 0 = raw reading valid
bit 1 = application calibration valid
bit 2 = obstruction active
bit 3 = timestamp valid
bit 4 = event flag/reserved event indication
```

Water stage is stored as a signed 32-bit integer because a valid local datum could produce a stage below zero.

The packet deliberately carries raw distance separately from derived stage.

## Important cloud status

The field firmware can now transmit this packet through Meshtastic.

The Heltec/Vercel/Neon cloud gateway still needs a distance-packet decoder before these `DS` packets become first-class water/trail records in the web dashboard.

Do not confuse **mesh transmission working** with **cloud ingestion already implemented**.

---

# 19. Manual telemetry test

Command:

```text
TELEMETRY NOW
```

The node performs a fresh distance read and queues a `DS` packet immediately.

This is useful for packet sniffing and gateway development.

---

# 20. Persistent configuration

Saved values include:

```text
schema magic/version
selected sensor
selected mode
water calibration reference
trail baseline
trail trigger delta
trail clear delta
report interval
daily count
lifetime count
UTC day key when known
```

The file includes a checksum. Invalid or incompatible saved data is ignored instead of being trusted blindly.

Normal recovery must preserve this file.

---

# 21. Self-recovery

Distance nodes use `DistanceSelfRecoveryModule` rather than the HOBO-specific BLE recovery module.

That distinction is important.

The HOBO recovery module actively manages Bluetooth scanning because HOBO loggers use BLE.

These distance sensors use I2C/UART, so the distance recovery supervisor does **not** manipulate the BLE scanner.

Implemented recovery commands:

```text
PING
WAKE
VERSION
UPTIME
POWER
BATTERY
WATCHDOG
RECOVER
REBOOT
```

The supervisor uses the same conservative philosophy as the HOBO field firmware:

- approximately 15-minute nRF52840 watchdog;
- do not steal watchdog ownership if something else already owns it;
- safe reboot after sending the reply;
- no NVS erase;
- preserve node identity;
- preserve channels/keys;
- preserve calibration;
- preserve saved counts.

---

# 22. Solar behavior inherited from the parent branch

The distance branches inherit the validated power changes from `field-self-recovery`.

## Seeed

The XIAO build retains:

- safer low-voltage battery curve ending around 3.40 V;
- nRF52 LPCOMP battery-rise wake configuration;
- ability to wake from deep SYSTEM OFF after solar recharge reaches the configured recovery threshold.

## RAK4631

The RAK build retains:

- existing RAK battery LPCOMP wake source;
- safer low-voltage margin around 3.40 V;
- the normal RAK recovery behavior.

Distance development must not remove those protections.

---

# 23. Direct-message command reference

## General sensor/application commands

```text
HELP
STATUS
HEALTH
SENSOR
SENSOR AUTO
SENSOR SEN0590
SENSOR SEN0311
SENSOR SEN0313
MODE WATER
MODE TRAIL
MODE IDLE
RAW
READ
TELEMETRY NOW
```

## Calibration

```text
CAL STAGE 1.42FT
CAL CLEAR
CAL RESET
```

## Trail tuning

```text
TRIGGER 12IN
CLEAR 6IN
COUNT
RESET DAILY
RESET COUNT
```

## Reporting

```text
INTERVAL 30SEC
INTERVAL 5MIN
INTERVAL 1HR
```

## Recovery / power

```text
PING
VERSION
UPTIME
POWER
BATTERY
WATCHDOG
RECOVER
REBOOT
```

Commands are intended as direct Meshtastic messages to the field node.

---

# 24. Example water installation

A field deployment can eventually look like this:

```text
1. Mount radio, antenna, solar, battery and distance sensor.
2. Aim the sensor at a stable patch of water surface.
3. Connect to the node through Meshtastic.
4. Send SENSOR SEN0313.
5. Send MODE WATER.
6. Send RAW several times.
7. Verify the readings match the approximate tape-measured sensor-to-water distance.
8. Measure current stage with the trusted field reference.
9. Send CAL STAGE 1.42FT.
10. Send READ.
11. Verify the returned stage matches the field measurement.
12. Send POWER.
13. Send WATCHDOG.
14. Set INTERVAL to the desired reporting interval.
15. Send TELEMETRY NOW and confirm the packet is heard elsewhere on the mesh.
```

Do not leave the site based only on one plausible number. Compare the sensor to a physical measurement.

---

# 25. Example trail installation

```text
1. Mount sensor on one side of trail.
2. Aim at a stable opposite target.
3. Keep people out of the beam.
4. Send SENSOR SEN0590 or the installed ultrasonic model.
5. Send MODE TRAIL.
6. Send RAW several times.
7. Confirm the empty-trail distance is stable.
8. Send CAL CLEAR.
9. Send COUNT.
10. Walk through once.
11. Send COUNT and verify +1.
12. Stand in the beam several seconds; verify it adds only one pass.
13. Clear the beam and pass again quickly; verify it rearms.
14. Test two people close together.
15. Adjust TRIGGER/CLEAR only if bench/field evidence says the defaults are wrong.
16. Send POWER and WATCHDOG.
17. Set the summary INTERVAL.
```

---

# 26. What still needs physical validation

The code now compiles, but the following must be bench-tested with the actual hardware before calling the firmware production-ready:

- SEN0590 I2C electrical wiring on Seeed;
- SEN0590 I2C electrical wiring on RAK19007;
- SEN0311 frame timing on Seeed;
- SEN0311 frame timing on RAK4631;
- SEN0313 frame timing on both boards;
- sensor supply current from the intended field power rail;
- exact behavior when target disappears;
- sunlight/water-surface behavior for ultrasonic water deployment;
- beam width and mounting-angle effects;
- trail detection speed;
- two people walking close together;
- person standing in beam;
- vegetation moving through the beam;
- count persistence across sudden battery removal;
- water calibration persistence across reboot;
- watchdog recovery;
- solar low-battery shutdown and automatic recharge wake;
- `DS` packet reception through the actual mesh path.

Compilation is milestone one. Hardware validation is milestone two.

---

# 27. Data-integrity rules

The firmware follows these rules:

1. **Keep raw distance.** Derived stage never replaces it.
2. **Do not fabricate readings.** A timeout is a timeout.
3. **Do not silently recalibrate.** Field calibration requires an explicit command.
4. **Keep calibration persistent.** A watchdog reboot should not change station geometry.
5. **Use hysteresis for trail detection.** One noisy threshold should not create repeated people.
6. **Do not send every trail sample over LoRa.** Fast sampling stays local.
7. **Version the telemetry packet.** Future gateway code can decode old/new packet formats safely.
8. **Preserve Meshtastic configuration.** Recovery is not a factory reset.

---

# 28. Source layout

```text
src/modules/Telemetry/DistanceSensor/
├── DistanceSensorDriver.h
│   └── common sensor-driver contract
│
├── DistanceSensorDrivers.h
├── DistanceSensorDrivers.cpp
│   ├── SEN0590 I2C implementation
│   └── DFRobot UART ultrasonic implementation
│
├── DistanceSensorModule.h
├── DistanceSensorModule.cpp
│   ├── commands
│   ├── persistence
│   ├── water calibration/stage
│   ├── trail state machine
│   ├── counters
│   └── DS telemetry packets
│
├── DistanceSelfRecovery.h
├── DistanceSelfRecovery.cpp
│   └── watchdog/power/recovery commands without HOBO BLE logic
│
└── README.md
```

---

# 29. Definition of field-ready

A distance firmware build should not be called field-ready merely because it compiles.

For a specific board + sensor combination, field-ready means:

```text
build passes
sensor communicates repeatedly
RAW agrees with a tape-measured distance
calibration survives reboot
mode survives reboot
battery telemetry works
watchdog is active
RECOVER preserves settings
LoRa telemetry is received by another node
solar recovery works on the intended power system
application-specific test passes
```

For trail mode, the application-specific test includes real people at realistic spacing.

For water mode, it includes multiple known distances/stages over the useful range.
