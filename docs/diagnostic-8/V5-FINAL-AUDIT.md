# RAK water-distance history and final V5 selection

Audit: October 15, 2026. Successful baseline: `cb632f61a45bcfbdf4501e56c9151d832eaf2e61` (Meshtastic `2.7.26.cb632f6`). Exact branch heads and relevant source blobs are recorded in [water-version-provenance.json](water-version-provenance.json). Earlier October 14 investigations remain in [README.md](README.md) and [SECOND-AUDIT.md](SECOND-AUDIT.md); this report supersedes their release recommendations where later evidence differs.

## Finding

The strongest supported explanation is the interaction between the switched ultrasonic sensor's blocking warm-up and the cooperative HOBO/mesh scheduler. V3 did not have this warm-up. V4 introduced Pololu switching and increased its blocking wait from 1 to 3 to 8 seconds. Early V5 inherited it. The last functional change before the reported MX2001 success replaced that wait with a timestamp-based job that returns to the scheduler, and serialized verification/calibration into a single power-on interval.

This is an identified scheduling defect and the leading explanation for the improvement. It is **not a controlled proof of the cause of every GATT failure**. The pinned Bluefruit event task and SoftDevice interrupts run independently; an Arduino `delay(1)` yields to those RTOS tasks, but does not return to Meshtastic's other cooperative `OSThread::runOnce()` jobs. Blocking the main controller is established. Starvation of the independent BLE event task is not established.

The supplied `meshtastic_debug_20261015_171915.txt` contains:

- October 15, 11:19: `HOBO universal STATUS model=MX2001 pointer=0x00001C15 interval=3600`.
- Historical October 14 failures: repeated service-discovery failure at candidate RSSI −32 to −39 dBm, plus disconnects and connection timeouts. The export mixes dates; those older entries are not failures observed after the successful build.
- User-reported successful MX2001 read after flashing the nonblocking build. The STATUS line independently verifies a decoded MX2001 protocol response and its one-hour logger interval. It does not by itself establish a correct stage value, repeated automatic hourly transmissions, or battery-only long-duration operation.

Earlier wake-only testing still failed with `hobo_wake=1` and `rsp=0 gatt=FFFF n=0`. Consequently, lack of CPU wake alone was not a sufficient diagnosis. The HFCLK request was retained in the successful package, so its independent necessity is unresolved. `FFFF` is the diagnostic sentinel for no observed discovery response, not an actual decoded GATT status.

## All RAK water-distance branch heads

Values below come from each branch's active module wiring and source. The water-only branches also contain universal HOBO source files, but their distance target excludes HOBO module construction. Dormant source is not runtime behavior.

| Branch family | Sensor power and read path | HOBO scan and polling | Recovery and significant differences |
| --- | --- | --- | --- |
| Water Distance V1 | SEN0313 default; no firmware-switched Pololu warm-up; 450 ms DFRobot UART frame limit | Water-only target; HOBO excluded | Distance recovery helper; no combined logger ownership |
| Water Distance V2 | Same distance module/driver as water-only V1 | Water-only target; HOBO excluded | Field V2 reset/radio maintenance and 12-hour fallback reboot |
| Water Distance HOBO V1 | SEN0313 default; no switched warm-up | Universal HOBO starts at passive 160/80 (50%); separate supervisor later sets 320/32 (10%); initial pointer checks 1 s, fine checks 0.5 s | Multiple scanner owners; supervisor scan kick every 30 min and stale-BLE reboot after 6 h; older 15 min field watchdog request |
| Water Distance HOBO V2 | Same unswitched sensor behavior; command-loop suppression | One scanner owner; passive 320/32 (10%); initial pointer checks 1 s, fine checks 0.5 s | 30 s connection-attempt limit; normal 90 s core watchdog and 12-hour maintenance reboot; scanner supervisor mutation removed |
| Water Distance HOBO V3 | SEN0313 default; no switched warm-up | Same passive 10%; STATUS checks standardized to 30 s | Bounded V3 discovery in deferred connect callback; distance calibration A/B slots and expanded water commands |
| Water Distance HOBO V4 | Adds IO1/P0.17 Pololu switching; 1→3→8 s blocking settling; MB7388 driver, 3 s frame limit, MB default; UART draining | V3 callback discovery and passive 10%; 30 s STATUS | Temporary 60 s bench default, then restored to 3600 s; temporary 10 s IO1 bench toggle removed; dangerous automatic MB migration could clear calibration |
| Initial V5 (`b24de43`) | Inherits V4's blocking 8 s wait; separate SEN0313/MB defaults | Changes scanning to passive 50%, periodic 30 s restart; discovery moved into cooperative main loop | More watchdog escalation and recovery deadlines; verified model history and explicit MAC lock added |
| Final selected V5 | Full 8 s settling through scheduled jobs; rail OFF between jobs; one rail owner; VERIFY/CAL use one warm-up; no automatic calibration-clearing migration | Restores passive 10% and callback discovery; no healthy periodic restart; 30 s STATUS and new-record-only automatic TX | Scanner race/stall fixes, peripheral-only phone shutdown, bounded wake/HFCLK, persistent identity/lock fixes, LED feedback; normal watchdog and 12-hour maintenance retained |

Safe HOBO V3 and V4 were also inspected. They have the same passive 10% acquisition and bounded callback discovery, without the combined ultrasonic warm-up. Their prior success is consistent with avoiding sensor/main-controller contention, but is not an isolated electrical test.

## Sensor power changes in order

| Commit | Change | Final choice |
| --- | --- | --- |
| `a0e4a7cb` | Pololu 2810 ON control on IO1; 1 s blocking settling; OFF after each read | Keep switching and OFF between jobs |
| `dceb355f` | Temporary 60 s bench water interval | Reject as a default; preserve each station's saved interval |
| `48c21cd6` | Temporary IO1 HIGH/LOW bench toggle every 10 s | Reject |
| `ee24657a` | Direct nRF52840 P0.17 register control; remove bench toggle; settling 3 s | Keep direct pin control |
| `5ee1f6d5` | Add MB7388 `Rdddd\r` parser and AUTO probe; fresh default MB7388; restore 3600 s default | Keep parser and two explicit build flavors |
| `014a3454` | Infer MB from a failed legacy UART read, switch type and clear calibration | Reject; sensor changes require explicit commands |
| `896e707f` | Drain MB warm-up UART data | Keep, serviced between scheduler returns |
| `a958b85a` | Increase settling from 3 to 8 s | Keep full duration, remove blocking execution |
| `e31cf1ed` | Nonblocking warm-up for scheduled/manual reads, STATUS, VERIFY, CAL, telemetry and AUTO; single sample owner | Keep |
| `cb632f61` | Do not turn rail OFF during an AUTO probe already warming; reject concurrent calibration mutations | Keep; successful baseline |

The electrical arrangement remains Pololu VIN on the existing 3.3 V supply, GND common, ON from RAK IO1/P0.17, VOUT to sensor power. Firmware uses the Pololu ON input with the physical slide switch OFF. This audit changes no wiring. It does not establish whether supply droop occurred in any failed field attempt.

## Selected operating package

| Setting | Final value | Reason |
| --- | --- | --- |
| Sensor settling | 8000 ms, nonblocking | Retain proven MB cold-start settling while allowing HOBO and mesh jobs to run |
| Sampling scheduler | 100 ms while busy, 1 s idle | Already present in successful build; drains UART during settling |
| Sensor power | ON for one job; OFF at completion/cancel; VERIFY 5 / CAL 7 valid samples within one window | Avoid repeated cold starts; bounded at 10 / 14 attempts |
| Driver frame limits | DFRobot 450 ms; MB7388 3000 ms | Retain successful parser behavior; these shorter reads still block the cooperative controller |
| Water interval | Saved value; fresh default 3600 s | No silent rewrite to 1, 5 or 15 min |
| Acquisition scan | Passive 320/32 BLE units: 200 ms period / 20 ms window = 10% | Five times less receive-window duty than the rejected 50% setting; not a fivefold whole-node battery-life claim |
| Healthy scan restart | None | Avoid interference with in-flight initiation and stale cached flags |
| Connection attempt | 30 s | Existing bounded attempt; no timeout inflation |
| GATT setup | 500 ms settle; at most 3 attempts per service/characteristic/notify; 350 ms retry gap | V3 callback sequence; library discovery wait remains 1 s |
| Protocol bootstrap | 30 s after GATT setup | Existing deadline |
| READ / STATUS response | 3 s | Existing deadlines |
| BLE retry | 5 s | Existing transient backoff |
| CPU wake and HFCLK | Claim before first idle sleep; at most 120 s; release on first valid STATUS baseline | Preserve successful package while preventing permanent wake hold |
| Failed wake budget | 300 s cooldown before another acquisition can claim wake | During cooldown normal platform idle sleep is permitted; passive scanning and mesh still run. This is not a five-minute radio shutdown |
| Connected operation | Normal idle sleep permitted; STATUS every 30 s | No permanent CPU wake lock; SoftDevice continues to manage BLE radio/clock needs |
| Automatic logger TX | On new logger record only | Initial STATUS baseline does not manufacture an hourly record; DM READ is separate |
| Watchdog / scheduled reset | 90 s core/radio protection; existing 12-hour preventive reboot | Preserve successful field recovery policy; no ordinary HOBO failure-triggered reset storm |
| Remote reset | REBOOT/RECOVER explicitly request watchdog-origin reset; SOFT REBOOT conventional | Preserve replay guard; neither is an electrical power cycle |
| LED feedback | 3 red flashes on failure; 3 simultaneous RGB flashes on valid handshake baseline | Existing nonblocking feedback; restore configured ambient lighting afterward |

HFCLK `STATS hobo_hf` indicates whether the crystal is running, not exclusive ownership by HOBO. Releasing the HOBO request does not stop a clock that the BLE stack needs. Similarly, `hobo_wake=0` means the HOBO sleep veto has ended; other platform activity may keep the CPU awake.

## What this finalization changes

The successful water, BLE, clock and power-control implementation is retained. The only runtime edit is identification: remove `HFCLK DIAG` from the V5 label and make `VERSION` include the compiled `APP_VERSION` and `APP_ENV`. Previously a VERSION DM could not distinguish successive V5 builds or field flavors. There is no new scan loop, longer timeout, sensor-power change or reset policy.

The finalization replaces conflicting front-page/companion documentation, adds this complete version review, and adds compiled execution tests of the production water-job methods with a fake clock/rail/driver. Those tests check scheduler return before the full 8 s deadline, busy-request exclusion, one power window, deferred replies, medians, finite failed calibration, AUTO paths, cancellation and millisecond rollover. They do not simulate RF success.

Both sensor applications and exact matched autonomous drone images must pass the workflow before publication. The normal station images are application-only and do not include SoftDevice, bootloader or filesystem addresses. Drone images embed the exact new field application and its DFU init packet. Physical calibration retention, repeated battery-only reconnection, hourly automatic records and actual airborne transfer remain distinct hardware checks. No new physical test is claimed by this finalization.
