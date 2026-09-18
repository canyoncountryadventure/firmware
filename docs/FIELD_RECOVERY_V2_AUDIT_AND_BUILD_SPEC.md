# Field-Recovery v2 — Final Audit, Architecture, and Firmware Build Specification

**Date:** 2026-09-18  
**Baseline:** Meshtastic 2.7.26 application tree with targeted reliability backports  
**Primary hardware:** RAK4631/RAK19007 and Seeed XIAO nRF52840 + Wio-SX1262  
**Purpose:** remote environmental stations that must recover autonomously from LoRa, BLE, scheduler, and partial peripheral failures.

## Executive summary

Field-Recovery v2 is the result of three firmware/reliability audits triggered by remote RAK4631 stations becoming unreachable after hours or days even though power was available. The failure pattern was not adequately protected by the previous self-recovery module because the CPU could remain alive while the SX1262 or HOBO BLE path stopped making useful progress.

The v2 design changes the definition of "healthy" from "the MCU loop is still executing" to "the MCU, LoRa radio, and applicable sensor transport are making progress." It also adds layered recovery so a single stuck state cannot strand a powered station indefinitely.

The common escalation model is:

```text
normal operation
  -> missed IRQ recovery
  -> radio RX retry
  -> SX1262 re-init
  -> SX1262 rail power-cycle + full re-init
  -> flash-safe whole-MCU reboot
  -> field watchdog channel deliberately starved
  -> hardware watchdog reset
```

HOBO builds add a separate BLE ladder:

```text
scan
  -> connect
  -> service/characteristic discovery
  -> STATUS / NEWREAD
  -> BLE disconnect/reconnect
  -> connect cancellation if stuck
  -> repeated recovery failure
  -> field watchdog reset
```

A 12-hour preventive reboot is retained during burn-in as an independent insurance mechanism. The production implementation is deliberately unconditional once due: it is not allowed to wait forever for an idle TX/sensor state, because a stuck queue or transport is exactly the condition this ultimate fallback must defeat.

---

## 1. Evidence and root-cause audit

### 1.1 Stock 2.7.26 radio core is shared by the custom firmware

The critical radio files in the original custom branches matched stock Meshtastic 2.7.26. The HOBO customization did not introduce the underlying SX1262 maintenance implementation.

The original shared core included:
- periodic SX1262 AGC/calibration maintenance every 60 seconds;
- RX missed-interrupt polling;
- a main-loop nRF52 watchdog;
- no complete SX1262 lost-state recovery ladder.

### 1.2 Confirmed unsafe SX1262 maintenance timing

The 2.7.26 `resetAGC()` path performs warm sleep, full calibration, image calibration, register restoration, and RX restart. Upstream Meshtastic later fixed a race where `CalibrateImage()` can continue internally after the API returns. Re-writing RX registers immediately afterward can fail write verification and leave the chip stalled.

Field-Recovery v2 therefore:
- waits 50 ms after `CalibrateImage()`;
- verifies return codes instead of blindly proceeding;
- treats failed maintenance as a radio-health fault;
- does not run maintenance during active RX/TX, queued TX, or pending IRQ activity.

### 1.3 Queued-TX race

The old maintenance guard checked active transmission but not traffic waiting in Meshtastic's TX contention queue. v2 exposes queue state and refuses invasive radio maintenance while queued traffic exists.

### 1.4 Missing TX interrupt safety net

The old build polled for a missed RX_DONE edge but not TX_DONE. v2 polls both.

### 1.5 CPU watchdog did not prove LoRa health

The old custom self-recovery module tried to configure its own 15-minute watchdog after Meshtastic had already started/owned the nRF watchdog. In normal operation, Meshtastic's main loop kept feeding the watchdog even when the external SX1262 could be nonfunctional.

v2 allocates a second nRF52840 watchdog reload channel before the watchdog starts:
- main loop channel = scheduler execution heartbeat;
- field channel = recovery/health heartbeat.

If radio or BLE recovery is exhausted, v2 deliberately stops satisfying the field watchdog channel. The hardware resets the entire nRF52840 even if the main loop is still executing.

### 1.6 SX1262 is separately powered hardware

On RAK4631, `SX126X_POWER_EN` is controllable by the MCU. A normal MCU reset is weaker than removing the SX1262 rail. v2 explicitly power-cycles the radio rail during recovery and also cold-starts it on boot.

### 1.7 HOBO BLE could enter permanent states

The original HOBO bridge could set `connecting=true`, stop scanning, and wait indefinitely for a connection callback. The scanner used a zero timeout, and no connection cancellation/deadline existed.

The original recovery module also inferred BLE health from whether the scanner was running. That was unsafe because the scanner is intentionally stopped while connecting/connected.

v2 therefore:
- gives the HOBO state machine sole ownership of scanner lifecycle;
- tracks a 30-second connection attempt deadline;
- calls `sd_ble_gap_connect_cancel()` when a connect attempt is stale;
- reconstructs the link after repeated STATUS/NEWREAD failures;
- escalates repeated BLE recovery failures to the field watchdog.

### 1.8 STATUS and NEWREAD were not functional heartbeats

Previously, repeated HOBO STATUS failures paused automatic transmission but could retry forever on a zombie connection. v2 treats repeated protocol failure as a dead link:
- 3 STATUS write failures -> BLE recovery;
- 3 STATUS response timeouts -> BLE recovery;
- 3 automatic NEWREAD timeouts -> BLE recovery;
- 5 BLE recovery cycles without stable progress -> hardware watchdog escalation.

### 1.9 nRF52 flash/reset safety

A separate upstream nRF52 issue showed that concurrent flash operations and resets during a pending page program can damage LittleFS and ultimately lose configuration or identity.

v2 intentional reboot paths therefore:
- lock SPI;
- lock InternalFS;
- flush the nRF52 flash cache;
- disable the active SoftDevice when present;
- only then perform the MCU reset.

### 1.10 nRF52 stack headroom

Current upstream nRF52 work demonstrated hardware-reproducible FreeRTOS task stack overflows during deep BLE/config paths. The custom firmware adds central-role BLE work for HOBO loggers, so v2 uses additional stack headroom:
- `LOOP_STACK_SZ=2048` words;
- `CFG_BLE_TASK_STACKSIZE=2048` words;
- `CALLBACK_STACK_SZ=2048` words.

### 1.11 Timer/reset diagnostics

The Arduino core caches/clears RESETREAS before normal setup. v2 uses `readResetReason()` instead of reading the cleared hardware register directly.

---

## 2. Field-Recovery v2 radio architecture

### Normal maintenance

Every second:
- poll RX_DONE when receiving;
- poll TX_DONE when transmitting.

Every 60 seconds:
- if RX was marked offline, attempt recovery first;
- otherwise consider guarded SX1262 maintenance.

Maintenance is skipped when:
- a packet is actively transmitting;
- a packet is waiting in the TX queue;
- an RX packet is actively in progress;
- the radio has a pending IRQ.

### SX1262 maintenance sequence

1. warm sleep;
2. RC standby;
3. full calibration;
4. wait for BUSY to clear;
5. image calibration for the configured frequency;
6. 50-ms image-calibration settle;
7. restore applicable DIO2/RF switch state;
8. restore RX boosted-gain selection;
9. restore the 0x8B5 RX-sensitivity patch;
10. restart RX;
11. any failed step enters recovery rather than continuing blindly.

### Radio recovery sequence

1. mark RX offline;
2. throttle recovery attempts so a dead chip cannot monopolize the scheduler;
3. full `begin()`/configuration sequence;
4. physically power-cycle `SX126X_POWER_EN` when available;
5. restore modem parameters, RF switching, gain, CRC, and RX register patch;
6. retry RX;
7. after repeated failures, stop satisfying the field watchdog heartbeat.

For RAK4631, this gives both MCU-level recovery and a true external SX1262 power cycle.

---

## 3. Watchdog architecture

Field-Recovery v2 uses the nRF52840 WDT as a final independent authority rather than merely an application-loop monitor.

### Main channel
Fed from the nRF52 main loop.

### Field-health channel
Allocated before WDT start. Normally fed while field recovery is permitted.

When a subsystem exhausts its recovery ladder:
- `nrf52FieldWatchdogTrip()` permanently disables field-channel feeding for that boot;
- the main loop may continue to run;
- because every enabled reload channel must check in, the watchdog eventually resets the MCU.

For the Field-Recovery v2 nRF52 build, the WDT runs during CPU sleep/halt because these environmental builds are not intended to enter multi-minute deep sleep while preserving an active HOBO/field runtime.

---

## 4. Preventive 12-hour reboot

A preventive reboot is included during initial field burn-in. It is not considered the primary fix.

Purpose:
- bound accumulated runtime state;
- rebuild the FreeRTOS/application state;
- rebuild SoftDevice/Bluefruit state;
- cold-start the SX1262;
- clear stale radio/IRQ/queue state.

Once long-term field data demonstrates that the explicit recovery layers are sufficient, this interval can be increased or removed.

---

## 5. HOBO BLE architecture

Supported HOBO family remains:
- MX2001;
- MX2201;
- MX2203.

The proven STATUS write-pointer + NEWREAD automatic-record design remains intact.

### Connection recovery
- passive 10% scan duty;
- HOBO module owns scanner;
- 30-second hard connection-attempt timeout;
- stale attempt canceled with the Nordic SoftDevice connect-cancel API;
- scanner resumes.

### Functional liveness
The node does not consider a link healthy merely because Bluefruit reports a connection. Actual STATUS/NEWREAD responses are the health signal.

### Recovery escalation
- repeated STATUS/READ failures -> disconnect/cancel/reconnect;
- repeated recovery cycles -> field watchdog trip;
- whole MCU reset reconstructs both SoftDevice and radio state.

### Remote commands
The v2 HOBO diagnostic module preserves the useful command surface:
- `PING`
- `STATUS` / `HEALTH`
- `LOGGER`
- `READ`
- `LOCK` / `UNLOCK`
- `POWER`
- `BLE`
- `AUTO`
- `WATCHDOG`
- `STATS`
- `NODES`
- `UPTIME`
- `VERSION`
- `RECOVER` / `REBOOT`

Scanner repair is automatic in v2; `SCAN`/`RECONNECT` explain that lifecycle is now state-machine controlled.

---

## 6. Sensor-product merge matrix

Each v2 branch is based on its matching v1 branch and then receives the shared hardening layer. Sensor modules are not replaced with a generic build.

| v2 branch | Board | Sensor/application functionality retained |
|---|---|---|
| RAK-HOBO-Safe-v2 | RAK4631 | HOBO MX2001/MX2201/MX2203 |
| RAK-Soil-Moisture-HOBO-v2 | RAK4631/RAK19007 | SEN0308 soil moisture + HOBO |
| RAK-Water-Distance-HOBO-v2 | RAK4631 | water-distance driver + calibration + HOBO |
| RAK-Water-Distance-v2 | RAK4631 | water-distance driver + calibration |
| Seeed-HOBO-Safe-v2 | XIAO nRF52840 + Wio-SX1262 | HOBO |
| Seeed-Water-Distance-HOBO-v2 | XIAO nRF52840 + Wio-SX1262 | water-distance + HOBO |
| Seeed-Water-Distance-v2 | XIAO nRF52840 + Wio-SX1262 | water-distance |
| Trail-Sensors-v2 | XIAO nRF52840 + Wio-SX1262 | PIR/Rock/HOBO build and dedicated SEN0171 trail counter |
| Remote-Drone-Flashing-v2 | RAK4631 | autonomous BLE embedded-target flasher + hardened target |
| Heltec-Gateway-v2 | Heltec V4 | Wi-Fi/mesh/Vercel/Neon gateway + existing gateway sensors/HOBO support + shared SX1262 recovery core (nRF52-only watchdog logic is not applicable) |
| field-self-recovery-v2 | RAK4631 + Seeed | canonical common self-recovery reference builds |

---

## 7. Reproducible builds

v2 pins important moving dependencies:
- Meshtastic nRF52 Arduino framework: `0fd295f13203e93df19d578073646ec32f2bf45a`;
- Meshtastic firmware GitHub build action: `39d0ffe8e0708beb3fb7b66f4c91aa941dc9764e` where directly invoked.

This prevents a later rebuild of a named v2 branch from silently using a different framework/action revision.

---

## 8. Build and publication policy

Each product branch:
1. builds on push to its v2 branch;
2. retains Actions artifacts for build inspection;
3. copies stable aliases into the default branch `downloads/` directory;
4. the default README links directly to those stable files.

For nRF52840:
- `.uf2` = USB bootloader installation/recovery;
- `-OTA.zip` = Nordic BLE DFU distribution package.

The default branch is the distribution catalog; product branches are the source/history.

---

## 9. Validation requirements

A branch is not considered release-ready merely because source exists.

Minimum CI:
- target compiles;
- expected UF2 generated;
- expected Nordic OTA ZIP generated for nRF52840;
- permanent stable aliases published.

Recommended bench/field validation:
- 7–14 day A/B soak;
- repetitive mesh TX/RX;
- HOBO reconnect/disconnect cycling;
- forced stale connection tests;
- forced SX1262 reset/power-cycle tests;
- manual RECOVER/REBOOT;
- verify config/key/logger-lock preservation;
- verify WDT reset reason;
- verify reboot recovery without USB intervention;
- compare 60-second AGC maintenance against longer intervals only after the fixed implementation is stable.

Remote Drone Flashing additionally requires a matched target/Scout pair from the same build.

---

## 10. What firmware still cannot recover

No firmware can guarantee recovery from:
- complete electrical power loss;
- physically failed MCU;
- physically failed SX1262;
- damaged antenna/feedline;
- corrupt bootloader;
- flash failure severe enough to prevent application boot;
- destructive physical/environmental damage.

The goal of v2 is narrower and practical: a powered, electrically functional station should not remain indefinitely unreachable because of an ordinary LoRa state loss, missed IRQ, stale BLE connection, stuck connection attempt, or recoverable application state.

---

## 11. Deployment policy

During burn-in:
- use v2 only on a station whose installation can tolerate a controlled reboot twice daily;
- retain the v1 artifacts as rollback images;
- use `STATUS`, `WATCHDOG`, and `STATS` after deployment;
- inspect reset/recovery counters in field telemetry;
- keep the 12-hour preventive reboot until long-duration results justify relaxing it.

After v2 proves stable in the field:
- evaluate 24-hour preventive reset;
- evaluate eliminating scheduled reset entirely;
- A/B test the 60-second AGC interval;
- consider upstreaming the generic portions that remain useful outside this project.

---

## 12. Source-of-truth branches

- `field-self-recovery-v2`: canonical common hardened baseline.
- Product `*-v2` branches: actual sensor-specific sources.
- `field-self-recovery`: default front page and stable downloads.
- v1 branches: preserved legacy/rollback sources.

Do not merge sensor products by replacing their branch trees with the canonical branch. The intended process is always **sensor branch first, shared hardening overlaid second**.


---

## 13. Upstream references used by the audit

These are the primary upstream reports and fixes that informed Field-Recovery v2:

- [Meshtastic firmware issue #10823 — RAK4631 becomes unresponsive on 2.7.26 while older firmware remains reliable](https://github.com/meshtastic/firmware/issues/10823)
- [Meshtastic firmware issue #8462 — RAK4631 LoRa communication stops while the MCU remains alive](https://github.com/meshtastic/firmware/issues/8462)
- [Meshtastic PR #9705 — SX126x periodic calibration / AGC-maintenance implementation](https://github.com/meshtastic/firmware/pull/9705)
- [Meshtastic PR #11774 — allow CalibrateImage to settle before restoring SX126x RX registers](https://github.com/meshtastic/firmware/pull/11774)
- [Meshtastic PR #11676 — recover SX126x runtime state loss with a full chip reinitialization](https://github.com/meshtastic/firmware/pull/11676)
- [Meshtastic PR #11678 — extend radio-state recovery into normal RX/TX paths](https://github.com/meshtastic/firmware/pull/11678)
- [Meshtastic PR #11872 — serialize nRF52 flash writers and quiesce flash before reset](https://github.com/meshtastic/firmware/pull/11872)
- [Meshtastic RAK4631 variant definition](https://github.com/meshtastic/firmware/blob/develop/variants/nrf52840/rak4631/variant.h) — confirms the separately controllable SX126x reset/power hardware used by the recovery ladder.
- [Adafruit nRF52 Arduino core reset implementation](https://github.com/adafruit/Adafruit_nRF52_Arduino/blob/master/cores/nRF5/wiring.c) — reference for reset reason caching and SoftDevice-aware reset behavior.
- [Nordic nRF52 watchdog documentation](https://docs.nordicsemi.com/) — reference for multiple watchdog reload channels and run-in-sleep behavior.

The project intentionally backports the narrowly relevant reliability changes instead of rebasing the entire sensor fleet onto an unvalidated major Meshtastic development branch.

---

## 14. Production release rule

A v2 source branch is considered deployable only when all of the following are true:

1. its current source tree retains the intended product/sensor module;
2. the shared radio/nRF52 recovery files match the canonical v2 recovery core;
3. its GitHub Actions build completes successfully;
4. the expected UF2 and/or DFU package is generated;
5. the stable alias is published into the default branch `downloads/` directory;
6. the default README contains the correct board/sensor description and download link;
7. the corresponding v1 branch remains untouched as a rollback baseline.

For HOBO and Remote Drone products, a successful CI build is still followed by physical field validation before replacing the known-good v1 image on every remote station.


---

## 15. Sensor-preservation verification

A final v1-to-v2 branch comparison was performed after the recovery layer was applied.

The important result is that the product-specific sensor implementations were **not replaced by a generic firmware tree**:

- **RAK Soil Moisture + HOBO:** the SEN0308 soil-moisture implementation and its product wiring/configuration remain from `RAK-Soil-Moisture-HOBO-v1`; v2 changes are confined to the common reliability layer, HOBO recovery, workflow, and documentation.
- **RAK Water Distance / RAK Water Distance + HOBO:** the `DistanceSensor` driver/calibration/persistence implementation remains from the matching v1 branches. The combined build additionally receives the HOBO v2 recovery changes.
- **Seeed Water Distance / Seeed Water Distance + HOBO:** the same existing water-distance driver and calibration implementation remains intact; combined builds add the HOBO v2 state machine.
- **Trail Sensors:** the PIR/Rock/HOBO and SEN0171 product code remains from `Trail-Sensors`; v2 changes are the shared nRF52/LoRa hardening and the v2 packaging workflow.
- **Remote Drone Flashing:** the autonomous Scout/flasher workflow remains, but the embedded target image is now built from the hardened v2 target so target and Scout remain a matched pair.
- **Heltec Gateway:** gateway ingestion, Wi-Fi, Vercel/Neon forwarding, direct HOBO BLE, and gateway sensor logic are retained. The v2 branch also receives the shared SX1262 calibration/IRQ/recovery core; only the nRF52840-specific dual-WDT and flash-reset code is omitted because Heltec V4 is ESP32-S3.
- **HOBO-only products:** MX2001/MX2201/MX2203 STATUS/NEWREAD behavior is retained, with intentional changes only to connection timeout, stale-link recovery, diagnostics, watchdog escalation, and removal of the temporary field-check test responder.

This verification is also reflected in the Git compare history: non-HOBO soil, distance, trail, and gateway sensor source files do not appear as replacements in the v1-to-v2 diffs.


---

## 16. Final CI verification

The final **source-changing** Field-Recovery v2 revisions were compiled in GitHub Actions on 2026-09-18. Every product target cleared its applicable compile/package path.

| Product | Final source validation | Result |
|---|---|---|
| RAK HOBO Safe v2 | RAK4631 build + UF2/DFU package + stable publish | PASS |
| RAK Soil Moisture + HOBO v2 | RAK4631 build + UF2/DFU package + stable publish | PASS |
| RAK Water Distance + HOBO v2 | RAK4631 build + UF2/DFU package + stable publish | PASS |
| RAK Water Distance v2 | RAK4631 build + UF2/DFU package + stable publish | PASS |
| Seeed HOBO Safe v2 | XIAO nRF52840 build + UF2/DFU package + stable publish | PASS |
| Seeed Water Distance + HOBO v2 | XIAO nRF52840 build + UF2/DFU package + stable publish | PASS |
| Seeed Water Distance v2 | XIAO nRF52840 build + UF2/DFU package + stable publish | PASS |
| Trail PIR/Rock/HOBO v2 | XIAO nRF52840 build + settings-safe OTA + stable publish | PASS |
| Trail SEN0171 v2 | XIAO nRF52840 build + UF2/DFU package + stable publish | PASS |
| Canonical Field Self-Recovery v2 RAK4631 | build + stable canonical aliases | PASS |
| Canonical Field Self-Recovery v2 Seeed | build + stable canonical aliases | PASS |
| Remote Drone Flashing v2 | hardened target build + embedded image generation + Scout build + package + stable publish | PASS |
| Heltec Gateway v2 | ESP32-S3/Heltec V4 build + package + stable publish | PASS |

The default branch was then re-audited after concurrent artifact publication:
- every firmware filename advertised in the front-page v2 tables exists in `downloads/`;
- the front-page README remains intact;
- the detailed audit document remains intact;
- all nRF52840 product branches share byte-identical common recovery-core files;
- Heltec Gateway v2 shares the same hardened radio-core files while retaining its ESP32-S3 platform layer.

Documentation-only commits made after these builds do not change firmware source or artifact contents.

## Completion audit — 2026-09-18

A branch-by-branch post-build audit was performed after the initial v2 rollout. The purpose was to verify that each product branch was a true upgrade of its existing sensor firmware, that the canonical recovery code was actually present, and that branch-specific behavior had not been lost during bulk merging.

### Audit method

For each product branch:

1. Compare the legacy/product branch against its v2 successor and verify the v2 branch descends from the intended product source.
2. Compare the shared radio/nRF52 files against `field-self-recovery-v2`.
3. Verify that sensor-specific modules remain present and were not replaced by generic firmware.
4. Inspect branch-specific watchdog, BLE, persistence, and reboot helpers for conflicting recovery ownership or unsafe reset/write paths.
5. Verify the GitHub Actions build/publish workflow and permanent-download path.
6. Correct documentation where it described obsolete v1 behavior or claimed capabilities not present in source.
7. Rebuild the exact corrected branch head and verify permanent artifacts.

### Repository-wide result

All current product lines now have v2 successors. Every product v2 branch is ahead of and zero commits behind its matching legacy/source branch. Legacy/v1 branches and artifacts remain available as rollback baselines.

The default `field-self-recovery` branch remains the front-facing firmware catalog. Its README contains permanent UF2/ZIP links for the complete v2 family, and the post-audit link check found no missing download targets.

### Branch results

| Branch | Result | Audit notes |
|---|---|---|
| `field-self-recovery-v2` | **PASS** | Canonical source contains missed RX/TX IRQ polling, bounded SX1262 recovery, guarded 60-second maintenance, 50 ms image-calibration settle, dual nRF52 WDT channels, flash-safe reboot, pinned nRF52 dependencies/stacks, and the 12-hour burn-in reboot. Canonical RAK + Seeed CI is green. |
| `RAK-HOBO-Safe-v2` | **PASS** | v1 HOBO behavior retained; common recovery/HOBO state-machine files match canonical v2. Documentation corrected for 30-second connection timeout, bounded STATUS/NEWREAD recovery, sole BLE scanner ownership, and dual-WDT behavior. Exact-head CI is green. |
| `RAK-Soil-Moisture-HOBO-v2` | **PASS** | SEN0308 soil implementation/calibration/raw telemetry retained; canonical v2 radio + HOBO recovery merged. Obsolete 15-minute/six-hour recovery documentation removed. Exact-head CI is green. |
| `RAK-Water-Distance-HOBO-v2` | **PASS** | Water calibration/persistence and HOBO behavior retained. The dormant DistanceSelfRecovery helper was updated to the v2 flash-safe reboot/watchdog interface so it cannot become an alternate unsafe reset path if instantiated later. Exact-head CI is green and permanent downloads were republished. |
| `RAK-Water-Distance-v2` | **PASS** | Fixed a real v1-era conflict: the water recovery helper directly called `NVIC_SystemReset()` and reported the legacy watchdog. v2 now reports the dual-WDT state and routes `RECOVER/REBOOT` through the centralized flash-quiesce + SoftDevice-disable reset path. Exact-head CI is green. |
| `Seeed-HOBO-Safe-v2` | **PASS** | Product behavior retained; canonical v2 radio/HOBO recovery present. Documentation corrected to match automatic BLE ownership and dual-WDT behavior. Exact-head CI is green. |
| `Seeed-Water-Distance-HOBO-v2` | **PASS** | Water + HOBO product behavior retained; dormant DistanceSelfRecovery helper updated to the same v2-safe path as RAK. Exact-head CI is green and permanent downloads were republished. |
| `Seeed-Water-Distance-v2` | **PASS** | Same direct-reset conflict found on the RAK water-only build was corrected. Exact-head CI is green. |
| `Trail-Sensors-v2` | **PASS** | PIR/Rock/HOBO and dedicated SEN0171 implementations remain intact. The PIR/Rock/HOBO image now uses the canonical v2 HOBO BLE state machine; the generic HOBO command supervisor is intentionally not instantiated because CCAStation already owns overlapping station commands. CCAStation now exposes v2 `PING`, `WATCHDOG`, and flash-safe `RECOVER/REBOOT`. CCA state/alert records and Rock calibration were corrected to truncate-and-replace with read-back verification instead of appending stale LittleFS records. Station version is `1.0.8-v2`. Trail CI now uses newest-run concurrency plus a source-head guard so stale runs cannot overwrite permanent downloads. Both Trail firmware builds, settings-safe OTA verification, and the permanent publish step are green. |
| `Remote-Drone-Flashing-v2` | **PASS — software/CI** | The audit caught an initial bulk-merge regression that had removed the proven target DFU-specific HOBO code. The `DFU` command, compact `VERSION`, persistent requester/build callback marker, post-update callback, and AdaDFU boot entry were restored on top of the hardened v2 BLE/radio core. The DFU marker now truncates and read-back verifies before bootloader entry; AdaDFU reset now quiesces flash and disables the active SoftDevice before GPREGRET/reset. Exact-head CI successfully built the target, generated the embedded image, built the matched Scout, packaged it, and published the stable v2 pair. The original `2.7.26.b812974` pair remains the physically proven reference; the new v2 pair still requires one repeat physical end-to-end bench DFU before remote deployment. |
| `Heltec-Gateway-v2` | **PASS** | Existing gateway/mesh/cloud/OTA behavior retained and shared SX1262 recovery code matches v2. nRF52-only dual-WDT logic does not apply to ESP32-S3. README/workflow wording corrected: direct local HOBO BLE remains a future Heltec gateway feature rather than a claimed current capability. Exact-head CI is green and the gateway ZIP was republished. |

### Important corrections found by the final audit

#### Water direct-reset bypass

The RAK and Seeed water-only branches still carried a product helper that scheduled its own direct `NVIC_SystemReset()`. That bypassed the new v2 pre-reset flash quiesce and SoftDevice shutdown. Under `FIELD_RECOVERY_V2`, those commands now schedule the centralized `rebootAtMsec` path instead. The legacy helper reset remains compiled only for non-v2 builds.

The same corrected helper was copied into both combined water + HOBO branches even though that helper is currently not instantiated there. This removes a dormant unsafe path rather than depending on it remaining unused forever.

#### Drone DFU regression and persistence hardening

The first bulk v2 merge preserved the drone workflow and README but accidentally lost the target's product-specific DFU implementation. This was not accepted as complete. The DFU additions from the physically proven drone branch were isolated and reapplied to the current hardened HOBO state machine instead of reverting the whole file.

The hardened target therefore keeps:

- direct-message `DFU` arming;
- persistent requester/channel/old-build marker;
- compact sub-payload-limit `VERSION` response;
- post-update old/new build callback;
- AdaDFU GPREGRET bootloader entry;
- current v2 BLE connection timeout and bounded STATUS/NEWREAD recovery;
- current SX1262 recovery and dual field watchdog design.

The callback marker originally inherited a LittleFS append hazard. It now truncates the stale record, writes the new record, reopens it, and verifies exact contents before allowing the node to leave Meshtastic for AdaDFU. The special AdaDFU reset also performs `nrf52FlashQuiesce()` and disables the active SoftDevice before setting GPREGRET and resetting.

#### Trail HOBO and persistence defects

The initial Trail v2 merge had the canonical LoRa/watchdog core but still carried the pre-v2 universal HOBO BLE state machine. That file was replaced by the canonical v2 implementation so Trail gains the 30-second connection cancel, bounded STATUS/NEWREAD recovery, and field-watchdog escalation.

The generic HOBO command/recovery supervisor is deliberately not instantiated in the Trail PIR/Rock/HOBO image because CCAStation already owns overlapping direct-message commands. Instead CCAStation directly exposes v2 liveness/watchdog/reboot diagnostics while the HOBO telemetry state machine remains the sole BLE scanner/link owner.

A separate persistence audit found that CCA station state, private alert destination, and Rock calibration all used Adafruit LittleFS `FILE_O_WRITE` as though it truncated the file. It opens an existing file at EOF. Repeated writes could therefore append records while the reboot loader continued reading the oldest record. All three persistence paths now truncate to one exact record and perform read-back verification.

Finally, repeated audit commits exposed a publication race: multiple Trail CI runs could finish out of order and write the same permanent filenames. Trail CI now cancels superseded runs and the publish job independently checks that `GITHUB_SHA` is still the current `Trail-Sensors-v2` head before touching permanent downloads.

### Validation boundary

A green CI build verifies source integration and artifact packaging. It does **not** replace hardware validation. In particular:

- the original drone `2.7.26.b812974` matched pair is the known successful physical DFU reference;
- the hardened drone v2 matched pair requires a repeat end-to-end bench DFU after this source merge;
- water sensor calibration/persistence, HOBO BLE behavior, soil analog response, trail detection/calibration, and gateway field behavior still require normal assembled-hardware checks before deployment;
- the 12-hour scheduled reboot remains intentionally enabled during v2 burn-in;
- v1 artifacts remain in the repository as rollback baselines.

The v2 repository policy is therefore: preserve the proven product behavior, add recovery underneath it, build every product independently, publish only from the current product head, and never treat a generic compile as proof that a product-specific field workflow still works.

