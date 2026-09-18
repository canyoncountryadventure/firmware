# Field Firmware v2 — Final Reliability Audit and Architecture

**Status:** canonical implementation specification  
**Date:** 2026-09-18  
**Base:** Meshtastic 2.7.26 codebase with targeted reliability backports  
**Scope:** every Canyon Country field firmware family: RAK4631, Seeed XIAO nRF52840, Heltec gateway, HOBO, soil moisture, water distance, trail sensors, and remote drone flashing.

## 1. Why v2 exists

Field nodes have shown a failure mode where they operate normally for hours or days and then stop participating in the mesh. The MCU can remain alive while LoRa traffic, remote commands, or HOBO telemetry stop. A conventional CPU watchdog is not enough because a healthy scheduler can continue feeding the watchdog while an external SX1262 or BLE state machine is wedged.

Three independent audits of the custom firmware, Meshtastic 2.7.26, current upstream Meshtastic changes, RadioLib/SX1262 behavior, Nordic nRF52840 watchdog behavior, Adafruit Bluefruit, and field telemetry produced the v2 design below.

## 2. Highest-confidence findings

### 2.1 Meshtastic 2.7.26 SX1262 calibration race

The exact radio-core files in the v1 RAK HOBO branch were verified byte-for-byte against stock Meshtastic 2.7.26 before custom v2 changes.

2.7.26 runs an invasive SX1262 maintenance sequence every 60 seconds. The sequence performs warm sleep, calibration, image calibration, register restoration, and receive restart.

Upstream Meshtastic later fixed a defect in this exact path: `CalibrateImage()` can continue internally after returning while BUSY does not remain asserted. An immediate register write can fail write verification and stall the chip. Upstream fix #11774 added a 50 ms settle delay before the post-calibration register writes.

**v2 requirement:** retain the maintenance concept, but include the 50 ms settle, guard it against queued TX / active RX, check every return code, and enter recovery immediately on failure.

### 2.2 2.7.26 lacks modern SX1262 lost-state recovery

Newer upstream Meshtastic added recovery for a chip that loses runtime state: re-run `begin()`, reprogram modem/RF settings, restart RX, and reboot only after repeated recovery failure.

**v2 requirement:** backport the self-contained recovery path without wholesale-moving the field firmware to the 2.8 alpha line.

### 2.3 Missed TX IRQ was not polled

2.7.26 polls missed RX_DONE edges as a backup, but did not poll missed TX_DONE in the same way.

**v2 requirement:** poll both RX_DONE and TX_DONE.

### 2.4 CPU watchdog != radio watchdog

The v1 custom supervisor attempted to create a 15-minute watchdog, but normal Meshtastic startup already owns the nRF WDT. The custom module therefore does not normally own the watchdog it reports.

More importantly, Meshtastic's main loop can remain alive while the SX1262 is dead. The main loop continues feeding the WDT, so the failure persists.

**v2 requirement:** use two nRF52840 WDT reload channels:
- channel 0: Meshtastic main loop
- channel 1: independent field-reliability supervisor

Both channels must check in for the WDT to reload. If subsystem recovery is exhausted and a software reboot does not complete, the field-reliability channel deliberately stops feeding and hardware resets the MCU.

HOBO builds keep the WDT running during CPU sleep because a persistent HOBO BLE link is not compatible with long unattended sleeps. Non-HOBO builds may retain pause-in-sleep behavior where intentional long sleep is part of the sensor design.

### 2.5 RAK4631 can cold-cycle the external LoRa radio

RAK4631 exposes:
- SX1262 NRESET
- `SX126X_POWER_EN`

An MCU reset alone does not prove the external radio rail dropped.

**v2 requirement:** recovery escalation is:

1. retry/re-arm RX
2. full SX1262 software re-init
3. SX1262 hardware reset through begin/re-init
4. actual `SX126X_POWER_EN` LOW → delay → HIGH → delay
5. rebuild radio state
6. flash-safe full MCU reboot
7. if software reboot fails, starve the independent WDT channel and force a hardware reset

On every RAK boot, v2 also forces a real radio rail cold start before initialization.

### 2.6 HOBO BLE had two real forever states

The v1 HOBO implementation could remain permanently stuck in at least two ways.

**Connecting forever:** the scanner is started with timeout 0, which means no timeout. The code sets `connecting=true`, stops scanning, initiates a central connection, and waits for callbacks. There was no application-level connection deadline.

**STATUS retry forever:** after repeated STATUS failures, auto telemetry paused and STATUS was retried indefinitely without tearing down the nominally-connected BLE link.

**v2 requirement:**
- 20–30 s absolute connect deadline
- cancel stale connect with `sd_ble_gap_connect_cancel()`
- three STATUS failures force BLE teardown/reconnect
- actual Bluefruit central connection state is checked
- the HOBO reader owns scanner/central operations; the generic supervisor does not manipulate Bluefruit scanner state

### 2.7 Scanner stopped is not proof of health

v1 self-recovery inferred:
- scanner running = disconnected
- scanner stopped = connected/idle

But the HOBO reader intentionally stops scanning before attempting a connection. A stuck connection attempt therefore appeared healthy to the supervisor.

**v2 requirement:** explicit functional states and health data replace scanner heuristics.

### 2.8 Intentional reset must be flash-safe

Current upstream Meshtastic fixed nRF52 LittleFS corruption caused by concurrent flash writers and reset during page programming. A reset during a page program can damage filesystem state and, in the worst case, lose configuration/identity.

**v2 requirement:** planned recovery and preventive reboots go through a flash-quiesce path before MCU reset. Custom modules must not directly call `NVIC_SystemReset()` for ordinary recovery.

### 2.9 Task-stack headroom

Current upstream nRF52 builds increased application/BLE task stacks after hardware-proven overflows. Field firmware adds persistent BLE-central work and recovery logic.

**v2 requirement:** pin a known nRF52 framework revision and explicitly provide stack headroom. Track heap/stack margin during soak testing.

### 2.10 Preventive reboot is valid field insurance

A powered environmental logger can keep recording while the mesh radio reboots. A short controlled reboot is preferable to a remote station remaining inaccessible for days or weeks.

The Thousand Lake history included a final failure before 24 hours of uptime, so a 24-hour fallback can lose the race.

**v2 burn-in policy:** approximately every 12 hours, with deterministic per-node jitter:
- wait for radio queue idle
- prefer a recent confirmed LoRa TX completion
- impose a maximum deferral
- perform a flash-safe reboot
- cold-cycle the SX1262 during next boot

After adequate field evidence, the interval can be lengthened or the preventive reboot removed.

## 3. Generic v2 field reliability module

Every v2 field build gets a common reliability supervisor.

Responsibilities:

- allocate an independent nRF WDT channel before WDT start
- heartbeat every 30 seconds
- track radio recovery exhaustion
- enforce the software-reboot → hardware-WDT-reset fallback
- enforce the 12-hour burn-in preventive reboot
- avoid reboot synchronization across a fleet using node-derived jitter
- never reboot solely because no remote packets were heard; RF silence can be legitimate

The supervisor does not decide that a radio is failed merely because there was no ACK from another node.

## 4. SX1262 health and recovery

The radio layer tracks:
- `lastTxCompleteMs`
- `lastRxCompleteMs`
- `rxOffline`
- soft recovery count
- hard recovery count
- consecutive recovery failures

Periodic radio maintenance:
- poll RX_DONE once per second
- poll TX_DONE once per second
- if RX is known offline, retry/recover instead of doing AGC maintenance
- otherwise run guarded AGC maintenance

AGC maintenance must not run when:
- a packet is transmitting
- a packet is waiting in the radio TX queue
- a packet is actively being received

Every operation involved in sleep / standby / calibration / image calibration / register restoration is checked. A failed operation enters the recovery ladder.

## 5. HOBO v2 behavior

Supported logger family remains:
- MX2001
- MX2201
- MX2203

Existing important behavior is preserved:
- logger lock persists
- STATUS write pointer drives automatic telemetry
- NEWREAD is only used when a new logger record exists
- manual READ does not consume the automatic pointer
- Meshtastic identity/channels/keys survive recovery

Added v2 behavior:
- explicit CONNECTING deadline
- stale connect cancellation
- functional STATUS heartbeat
- BLE teardown/reconnect after repeated STATUS failure
- HOBO reader is sole owner of scanner/central connection
- remote RECONNECT requests the same coordinated recovery path
- remote REBOOT/RECOVER schedules the common flash-safe reboot
- diagnostics expose BLE state and recovery counters

## 6. Sensor merge rule

Reliability code is a common lower layer. Sensor drivers remain branch-specific.

### RAK HOBO Safe v2
Universal HOBO reader + full RAK radio/WDT/BLE reliability.

### RAK Soil Moisture HOBO v2
Everything in RAK HOBO Safe v2 plus SEN0308 soil moisture behavior and its threshold/telemetry functions.

### RAK Water Distance HOBO v2
Everything in RAK HOBO Safe v2 plus water-distance sensor drivers and distance self-recovery.

### RAK Water Distance v2
RAK radio/WDT/12-hour reliability plus water-distance sensor stack; no HOBO BLE requirement.

### Seeed HOBO Safe v2
Universal HOBO BLE behavior + generic nRF WDT/12-hour reliability. SX1262 recovery applies through the common radio layer; RAK-only radio rail power control is compiled only where the pin exists.

### Seeed Water Distance HOBO v2
Seeed HOBO v2 plus water-distance sensor stack.

### Seeed Water Distance v2
Seeed field reliability plus water-distance stack.

### Trail Sensors v2
Preserve PIR/Rock/HOBO and SEN0171 variants. Apply the common nRF/SX1262 reliability layer to both builds. HOBO-specific BLE logic only compiles into the HOBO variant.

### Heltec Gateway v2
Preserve Heltec gateway/cloud-ingest functions. Apply common SX1262 maintenance/recovery where supported. nRF-only watchdog logic is excluded.

### Remote Drone Flashing v2
Preserve the verified remote-drone-flashing function and embedded target images. The drone/flasher radio itself receives the applicable reliability layer. Embedded v2 target images must be generated from the v2 production artifacts, not stale v1 files.

## 7. Production cleanup

v2 production builds must:
- remove HOBOFieldCheck test/joke module
- pin GitHub Action commits
- pin nRF framework revision
- identify branch/build/source revision in VERSION
- avoid moving dependencies such as workflow `@main`
- keep v1 branches/artifacts intact for rollback

## 8. Build and release policy

Each firmware family receives a v2 branch.

Expected source branches:
- Heltec-Gateway-v2
- RAK-HOBO-Safe-v2
- RAK-Soil-Moisture-HOBO-v2
- RAK-Water-Distance-HOBO-v2
- RAK-Water-Distance-v2
- Seeed-HOBO-Safe-v2
- Seeed-Water-Distance-HOBO-v2
- Seeed-Water-Distance-v2
- Trail-Sensors-v2
- Remote-Drone-Flashing-v2
- field-self-recovery-v2

The default `field-self-recovery` branch remains the front-facing catalog and permanent download surface.

Every source workflow must:
1. build its exact v2 source branch
2. upload normal Actions artifacts
3. copy the stable user-facing UF2 / Nordic DFU ZIP into `downloads/` on the default branch
4. use v2 names
5. use pinned Action SHAs
6. fail if expected UF2/ZIP is absent

## 9. Required soak test

Before declaring v2 production:
- minimum 7-day bench soak; target 14 days
- continuous mesh traffic plus quiet periods
- repeated HOBO intervals
- forced BLE out-of-range/reconnect cases
- intentionally cancel/interfere with HOBO connection attempt
- forced STATUS failures
- radio TX/RX traffic during AGC-maintenance windows
- verify missed TX/RX IRQ recovery
- deliberately invoke radio soft recovery
- deliberately invoke radio rail power-cycle recovery on RAK
- verify 12-hour preventive reboot
- verify logger lock and Meshtastic identity survive reboot
- verify reset reason and counters after reboot
- monitor free heap and stack watermarks
- verify no filesystem/key/config loss

## 10. Release acceptance criteria

A v2 artifact is production-ready only if:
- its branch Action is green
- expected UF2/DFU ZIP is published
- boot succeeds without factory erase
- LoRa TX and RX succeed
- applicable sensor readings succeed
- applicable HOBO auto pointer behavior succeeds
- remote STATUS works
- remote RECONNECT works where applicable
- remote RECOVER schedules a flash-safe reboot
- scheduled preventive reboot returns to mesh automatically
- radio hard recovery does not alter identity/channels/logger lock

## 11. Philosophy

No single software failure should be able to strand a powered field node indefinitely.

The layers are deliberately independent:

```text
sensor/HOBO functional recovery
        ↓
LoRa IRQ/RX recovery
        ↓
SX1262 full re-init
        ↓
SX1262 hardware reset
        ↓
SX1262 rail power-cycle (where available)
        ↓
flash-safe whole-node reboot
        ↓
independent WDT channel starvation
        ↓
hardware reset
```

Physical hardware failure, actual loss of electrical power, damaged flash/bootloader, or a physically failed radio can still require a site visit. Ordinary firmware, BLE, scheduler, IRQ, and SX1262 state failures should not.
