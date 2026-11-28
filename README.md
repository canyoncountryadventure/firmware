# Heltec Gateway V13

Moab watershed gateway firmware for the **Heltec WiFi LoRa 32 V4 OLED, ESP32-S3**, PlatformIO target `heltec-v4`.

[Download the published application BIN][application-bin] · [Companion ZIP][companion-zip] · [SHA-256 and source commit][build] · [Build status][workflow]

These links follow the last successful V13 publish. The dated filenames and source commit in BUILD.txt identify exactly which revision you are downloading. [Flash and rollback notes](downloads/Heltec-Gateway-v13/README.md)


## Field incident record — November 27, 2026 (MST)

**Current candidate: V13 R12, installed and provisionally healthy. Do not confuse a successful build, a confirmed OTA boot, or early healthy telemetry with a completed endurance test.** This record is deliberately dated; retain it when subsequent releases are built.

### What failed and why we changed direction

| Revision / CI run | Outcome and lesson |
| --- | --- |
| [R1](https://github.com/canyoncountryadventure/firmware/actions/runs/45898210761) | Passed CI; bound TLS/network stalls and retain LoRa/AUTO-DM. |
| [R2](https://github.com/canyoncountryadventure/firmware/actions/runs/45917438004) | Passed CI; centralized reconnects and bounded network liveness. |
| [R3](https://github.com/canyoncountryadventure/firmware/actions/runs/45925210489) | Cancelled; superseded REBOOT DM work. |
| [R4](https://github.com/canyoncountryadventure/firmware/actions/runs/45925390610) | Passed CI; any-radio REBOOT. Field: network recovery induced repeated software resets (boots 39–49). |
| [R5](https://github.com/canyoncountryadventure/firmware/actions/runs/46067452651) | Passed CI; separate backend reachability and repair Wi-Fi without immediately rebooting. Did not eliminate HTTPS outages; TLS -16 was observed. |
| [R6](https://github.com/canyoncountryadventure/firmware/actions/runs/46079250846) | Failed CI; initial retained TLS recorder build. |
| [R7](https://github.com/canyoncountryadventure/firmware/actions/runs/46079855869) | Cancelled; target configuration/recorder guard correction. |
| [R8](https://github.com/canyoncountryadventure/firmware/actions/runs/46079920892) | Passed CI; recorder target-detection regression test. Oversized DIAG reply remained. |
| [R9](https://github.com/canyoncountryadventure/firmware/actions/runs/46084634929) | Passed CI; packet-safe paged DIAG/CRASHLOG. Field TLS recording exposed allocation failure; HTTPS still stopped. |
| [R10](https://github.com/canyoncountryadventure/firmware/actions/runs/46093111757) | Failed link: gateway feature macro tested before its defining header; allocator translation unit excluded. |
| [R11](https://github.com/canyoncountryadventure/firmware/actions/runs/46093858374) | Failed compile: Arduino EXTERNAL macro collided with allocator capability constant. |
| [R12](https://github.com/canyoncountryadventure/firmware/actions/runs/46094432048) | Passed compatibility, ESP32-S3 compile/link and publication; installed and preliminary field verification successful. |

Rebooting restored HTTPS temporarily, not permanently. Wi-Fi rejoin/driver resets, extra watchdog escalation and per-service recovery could not cure the underlying TLS allocation problem. R4's broad reboot policy also interrupted the missing-AUTO timers, which need a full hour of uptime. Separating service reachability and preserving LoRa during Wi-Fi repair were useful corrections worth retaining, but not sufficient fixes on their own. R9 field traces showed strong RSSI (about -63 dBm), an IP address and roughly 48–52 KB free internal heap during the outage, yet the lifetime low reached 204 bytes and the TLS error was -17040 (-0x4290, RSA public operation plus MPI allocation failure). The available PSRAM was approximately 1.99 MB. A healthy-looking idle heap did not measure the transient handshake allocation peak. The evidence supports internal allocation pressure; it does not prove a memory leak or explain every interrupt-watchdog reset.

### What appears to have worked

R12 uses the real pinned Arduino-ESP32 2.0.17 SDK Mbed TLS runtime allocator hook, not only a preprocessor setting. It installs after PSRAM initialization, probes the actual linked allocator, prefers byte-addressable PSRAM for Mbed TLS allocations, pairs allocations with capability-aware free, and keeps a best-effort 24 KiB internal reserve for fallback. Failed HTTPS requests are also paced independently by destination, with a shared five-second settling interval. Existing short-lived TLS clients and cleanup remain. These two changes shipped together, so the field observation is not an isolated A/B test; the allocation counter and handshake memory traces directly support the allocator mechanism.

Source breadcrumbs: [allocator implementation](https://github.com/canyoncountryadventure/firmware/blob/a7aa6b6f98d87ef17b37ab5fdd88358cb51fa08f/src/modules/CcaTlsAllocator.cpp), [retry gate](https://github.com/canyoncountryadventure/firmware/blob/a7aa6b6f98d87ef17b37ab5fdd88358cb51fa08f/src/modules/CcaTlsRetryGate.h), [integration checks](https://github.com/canyoncountryadventure/firmware/blob/a7aa6b6f98d87ef17b37ab5fdd88358cb51fa08f/test/test_heltec_v13/test_tls_allocator_integration.py), [exact build metadata](https://github.com/canyoncountryadventure/firmware/blob/18847a4b324325e8ccdaa0590a14f1e25813cf9a/downloads/Heltec-Gateway-v13/BUILD.txt).

### OTA history and field evidence

- R9 installed on November 27 at 5:04:58 PM MST. Boot 58 produced the decisive TLS allocation evidence. Boot 62's second R12 OTA attempt failed with HTTP/TLS -1 and errno 11; repeated HTTPS failures followed. An HTTP 200 during download alone did not prove application installation.
- R12 approval: configuration revision 24, 6:49:19 PM MST. Unique release label: `heltec-v13-tls-psram-r12-20261127`.
- R12 verified boot: 9:04:49 PM MST, boot 65; phase 3 followed by phase 4, attempts 0. Cloudflare automatically disabled OTA and advanced configuration to revision 25. Historical INT_WDT(5) was reported at boot 65; no later reset is shown in this snapshot.
- As of the 9:55:00 PM MST heartbeat (2026-11-28 04:55:00 UTC), uptime was 3,040 s (50m 40s), same boot 65; HTTP 200/201, empty upload queue, last upload age 5 s, network repair state 0. Earlier V13 connected uptime in the available health history peaked at 934 s (15m 34s).
- `tls_psram=1`, `tls_ps_allocs=484257`, `tls_int_fallback=0`, `tls_alloc_fail=0`. Allocation count is the number of allocation calls, not concurrently live allocations. Retained failure counters did not increase from the first R12 heartbeat; last recorded failure belongs to boot 63, before the verified installation.
- R9 handshake sample (boot 62, sequence 345): 59,264 B before connect, 16,852 B after handshake; lifetime minimum 3,524 B at that sample. R12 sample (boot 65, sequence 868): 54,020 B before connect, 53,568 B after handshake, largest block 40,948 B, lifetime minimum 41,860 B. R12's PSRAM recovered to 1,986,687 B after cleanup. Compare same-boot records, not cumulative failures or lifetime minima across resets.

### How to reproduce the investigation

In the Cloudflare D1 database `cces-monitor`, filter `gateway_id='heltec-moab'`:
- `mesh_gateway_health`: heartbeat uptime, HTTP result, queue depth, applied image, recovery state and allocator counters in `note`.
- `mesh_gateway_tls_diag`: numeric retained records with boot, operation, TLS/HTTP errors, before/after handshake/HTTP/cleanup memory; schema/index mapping below.
- `mesh_gateway_reset_diag`: previous reset reason/event and retained first/latest network-repair evidence.
- `mesh_gateway_config`, `mesh_firmware_releases`, `mesh_firmware_actions`: desired OTA manifest, immutable build identity, approval and verified-boot history.
- `mesh_gateway_dm_state`: actual device-reported enabled watches, telemetry/request ages and configured slots; `reported_at` and `snapshot_json`. Cross-check accepted `heltec_dm` sensor readings in Neon `moab_watersheds`, `public.telemetry_readings`, rather than assuming a queued radio request was delivered.

### Remaining acceptance tests / frozen scope

- Continue overnight without changing the firmware. Require at least two hours of uninterrupted connected uptime and successful ingestion; then observe a full twelve-hour preventive-reboot cycle separately. A scheduled reboot is not itself an unexpected crash.
- Verify actual missing-AUTO DMs at the first eligible clock boundary (11 PM MST for the approximately 9:04 PM boot and hourly watches); no DM is required when recent valid AUTO data exists. At 9:55 PM the snapshot had six enabled streams across five stations, all hourly, and no fallback requests yet. Pack Creek has two streams; Mill Creek, Matheson, Cottonwood Relay and Fishlake each have one.
- Preserve `DIAG`, `DIAG 2`, subsequent pages, `CRASHLOG`, any-radio `REBOOT`, and all existing station/LoRa/OTA settings. R12 retains paged diagnostics in source and CI; actual field DM reply delivery has not yet been confirmed.
- Four legacy persistent recurring-poll slots are separate from the eleven missing-AUTO watches. All four were empty in the snapshot. R12 reports them to Cloudflare but does not accept remote configuration for them. Future dashboard scheduling would require new firmware support; explicitly deferred until stable. Do not alter the currently running device for this documentation update.
- If failures recur, capture traces before rebooting when possible. Compare the post-cleanup heap, largest internal block, lifetime minimum, PSRAM counters and failure stage. Do not repeat blind watchdog changes or claim every reset is resolved.

## Next firmware and dashboard backlog

**Planning only — not implemented in R12.** Keep the installed R12 binary and all historical evidence unchanged until overnight stability and real automatic-DM delivery have been verified. The following items are requirements or proposed enhancements for the next intentionally approved release, not instructions to flash now.

### Priority 1 — keep the repair; reduce diagnostic overhead safely

- Preserve the real PSRAM allocator hook, runtime probe, capability-aware free, 24 KiB best-effort internal fallback reserve, per-destination retry pacing, separate upload queues, recovery limits and LoRa-first DM dispatch. Do not remove these to save bytes.
- Keep the five-minute configuration GET and a compact five-minute health POST. They serve remote control, OTA acknowledgement, online status and applied schedule verification, not merely troubleshooting. Do not lengthen configuration polling when the operator expects settings within five minutes.
- After R12 endurance acceptance, decouple verbose diagnostics from the heartbeat. Proposed normal mode: keep cheap fixed-size RAM/RTC recording around HTTPS and always report compact uptime, boot/reset reason, memory free/largest/minimum, PSRAM allocation/fallback/failure counters, queue depth, endpoint status and config acknowledgement. Send the full retained TLS trace and reset detail on a new boot, new failure, new diagnostic event, requested capture or a periodic 60-minute summary. Keep the five-minute verbose mode selectable for troubleshooting. Never drop first/latest failures or the previous reset event before confirmed delivery; deduplicate by boot and sequence/event ID at the server. Retain full traces on-device when offline; no extra HTTPS session per diagnostic event. Preserve backward-compatible Worker parsing and the 8 KiB health limit. Test verbose/compact switching, history retention, and reduced payload size; do not assume smaller JSON alone fixes TLS.
- Dashboard correction (can be shipped independently, after review): the Last ESP32 reset card currently shows the receipt time of a repeatedly reported historical reset. Example: the 10:05 PM MST report still said boot 65, but uptime was 3,643 s (1h 43s), proving there was no new 10:05 PM reset. Display boot counter, estimated boot time (heartbeat UTC minus reported uptime), reset cause, and last report time separately. Label estimates; use earliest matching boot evidence when available, handle clock uncertainty, delayed reports and scheduled resets, and never advance reset time just because another heartbeat arrived.

### Priority 2 — four independently scheduled DM slots

R12 already has four persistent recurring slots, a fifth one-time manual slot and a sixth one-shot missing-AUTO rescue slot. All four persistent slots are currently empty; the remote snapshot exposes them but configuration parsing does not accept them. Do not confuse this feature with the existing eleven independent missing-AUTO sensor watches.

- Add a dedicated Scheduled DM Polling section in Cloudflare, separate from missing-AUTO controls. Four rows: enabled/off, known station, interval, sensor mode READ / WATER / BOTH (only where supported), desired versus device-confirmed values, last request/reply/result and next due time when reliably available. Default all four OFF. Recurring polling requests readings even when valid AUTO is already arriving; explain possible extra airtime and battery usage.
- Add a versioned, authenticated remote config field for the four slots, independently revisioned from missing-AUTO policy. Validate known node IDs, allowed sensor modes and bounded intervals on both Worker and firmware; reject duplicates for the same sensor and malformed/partial updates atomically. Existing recurring scheduler currently uses elapsed time and supported interval values 15/30/45/60/360/720/1440 minutes; proposed behavior is to retain elapsed scheduling, document whether first poll is immediate on enable, and require explicit confirmation before adding extra interval values or changing to wall-clock scheduling. Missing-AUTO remains clock-aligned with a full one-hour prerequisite.
- Apply on the next successful five-minute config poll, no reboot. Save only changed persistent settings to NVS, preserve settings offline and through reboot/OTA, acknowledge actual applied revision in health, and avoid rearming unchanged watches or restarting the poll timer on every config fetch. Use a coherent snapshot under the existing synchronization model; only the gateway scheduler mutates active poll state.
- Serialize all LoRa polling through the existing one-job-at-a-time dispatcher, preserve spacing/retries, prioritize eligible rescue jobs, avoid duplicate requests when schedules overlap, and prevent stale late replies from being assigned to a replacement target. Scheduled OFF must stop that slot without disabling AUTO or fallback. A one-time POLL remains independent. Verify the temperature-only routing for scheduled READ as well as rescue READ; Pack Creek WATER and HOBO READ remain distinct. Report accepted replies as heltec_dm through the proper existing station destination; a queued RF request is not a successful reading.
- Test enable/change/off, all four slots, conflicting changes, config revision replay, lost ACK, offline reboot persistence, manual/fallback concurrency, no duplicate jobs, supported and rejected station modes, and real accepted replies in both appropriate backends.

### Priority 3 — two useful radio diagnostic packets, not three

Current formatter uses 170-byte text slices and a 200-byte reply budget. The old 229-byte reply exceeded the 255-byte encrypted radio packet limit; do not regress that correction. User requirement: one DIAG request should return at most two compact, numbered diagnostic packets automatically, rather than requiring three separate page requests. Target exactly two when both sections contain meaningful data.

- Page 1: firmware/release, boot count, uptime, last reset reason/event, Wi-Fi IP/connected flag, RSSI, HTTP/config freshness, queue depth. Page 2: free/largest/minimum internal RAM, PSRAM available and active flag, allocation/fallback/failure counters, last meaningful network/TLS failure and auto-DM/poll status summary. Use concise field names, fixed bounded formatting and explicit units; no essential data may disappear silently. Full detailed history remains in Cloudflare and an optional explicit extended diagnostic command if needed.
- Capture one consistent diagnostic snapshot and queue page 2 behind page 1 with existing LoRa pacing (not simultaneous sends, blocking delay, or unbounded retries). Retain DIAG 1 / DIAG 2 to request an individual missing page and CRASHLOG compatibility; old DIAG 3 can return a short redirect to the full detail, not a third automatic packet. Read-only diagnosis must work while Wi-Fi is down. Rate-limit repeated requests to prevent airtime flooding without blocking legitimate manual requests.
- Test worst-case numeric values, exact wire serialization with PKI/LongFast, protobuf/header overhead, 200-byte text limit and 255-byte wire limit, two-page completeness, delivery ordering, queue-full behavior and real remote radio reception. Do not increase packet size to make the text fit.

### Priority 4 — command discoverability and remote field tools

Keep the existing any-radio direct-message trust model for authorized gateway-control commands, including REBOOT (20-second delayed restart and acknowledgement), AUTO DM STATUS, AUTO DM OFF, AUTO DM <station> <interval|OFF>, POLL <station> [READ|WATER|BOTH], and DIAG/CRASHLOG. Valid direct PKI or LongFast messages only; never act on broadcasts. Audit existing command dispatch before adding aliases.

Proposed new commands (syntax to be finalized and tested; not available in R12):

| Command | Intended behavior |
| --- | --- |
| HELP | Compact command index; separate HELP DM / HELP DIAG / HELP SCHED if needed to fit radio budget. |
| STATUS | One compact reply: uptime, boot, installed revision, Wi-Fi, recent HTTP status, queue count and allocator status. |
| NET | IP-present flag, RSSI, config/health age, backend status, recovery state and most recent transport error. No credentials. |
| MEM | Internal free/largest/minimum, PSRAM free, allocator state and fallback/failure counts. |
| SCHED STATUS | Report four recurring slots, modes, intervals and due times compactly. |
| SCHED <1-4> <station> <interval> <mode> | Configure one persistent recurring slot with the same validation and conflict policy as dashboard. |
| SCHED <1-4> OFF | Disable only that slot; preserve missing-AUTO settings. |
| SYNC | Request an expedited authenticated config poll using existing retry gate and a cooldown; report queued versus applied, never bypass OTA verification. |
| DM STATUS <station> | Actual fallback enabled state, interval, last AUTO/request age, eligibility and recent reply outcome for a single station. |

For local SCHED changes versus cloud desired state, design explicit revision/ownership reconciliation and display conflicts; do not silently undo radio changes on the next poll or allow stale config to overwrite a newer local change. No unauthenticated credential disclosure, arbitrary shell commands, automatic firmware download by DM, or added reboot privileges. Keep command acknowledgements packet-safe and useful without internet.

### Priority 5 — reliability follow-up and release gates

- Fix misleading historical reset timestamp; show last successful ingestion, last config application, actual last AUTO/DM reply and schedule age distinctly. Display memory trends and current boot counters without treating cumulative historic failures as new incidents.
- Review the blanket twelve-hour preventive reboot only after observing a complete stable cycle and collecting enough evidence. Consider removing or making it conditional if no longer needed; do not change it in the next release by default. Likewise do not blindly increase watchdog timeouts, force Wi-Fi reconnects, change TLS keepalive, or clear RTC diagnostics.
- Preserve all node IDs, radio settings, sensor decoding, calibration, credentials, upload routes, manual controls, signed OTA and recovery loader. Keep the known R12 application BIN/ZIP and exact source hash as rollback reference. Avoid broad source changes; compare each next-release diff to R12.
- Require successful real ESP32-S3 compile/link and compatibility tests, bounded diagnostics packet tests, remote config validation and persistence tests, full scheduler replay, heap/queue recovery tests, and physical-radio delivery checks. Verify unique OTA label/hash, boot confirmation and automatic OTA closure. Test a two-hour healthy connected run, actual eligible fallback plus scheduled DM replies, and an overnight/preventive-reboot cycle before calling the new release stable. If practical, stage scheduling and diagnostic changes separately so regressions are attributable. Maintain both READMEs with exact artifact links and dated evidence.

### Priority 6 — adaptive stream-stage sampling during high-water events

Operator proposal (November 27, 2026): keep normal stream-station measurements and transmissions hourly; when a valid new stage reading increases by **more than 100%** relative to the previous valid reading, switch to a 15-minute measurement/reporting interval. Continue event mode until stage is at or below 90% of the fixed reading that triggered event mode, then return to hourly sampling. These definitions are proposed for operator confirmation before implementation; "trigger value" is interpreted here as the new triggering stage, not the pre-event baseline or subsequent event peak.

| State | Rule |
| --- | --- |
| Normal | Sample and report every 60 minutes; compare the new valid calibrated stage H with the preceding valid stage P. |
| Enter event | H > 2 × P (strictly more than 100% increase), with valid positive baseline and minimum absolute-rise checks. Save T = H as the fixed event trigger level; immediately report the trigger reading and change to 15-minute sampling. |
| Event active | Sample/report every 15 minutes, preserving timestamps and the original trigger T; do not replace T with each new reading or peak. |
| Exit event | H <= 0.90 × T; report the recession reading, then restore the 60-minute interval. |

Example: previous stage 1.00 ft, new stage 2.20 ft (120% increase). Event sampling begins. The fixed exit threshold is 1.98 ft; once a valid reading is at or below 1.98 ft, return to hourly. A reading exactly twice the prior stage is not strictly greater than 100%. This proposed exit condition can end rapid sampling while stage is still substantially elevated relative to pre-event conditions; confirm whether the operator instead wants recession to 90% of the pre-event stage or a fixed site-specific threshold before coding.

Implementation notes:

- Implement the adaptive measurement/reporting state machine at each compatible field stream station (RAK/Seeed or other station firmware), not solely in Heltec. Preserve operation during gateway, mesh and internet outages. For external loggers verify whether a fresh measurement can actually be taken on demand; reporting cached hourly data every 15 minutes is not high-frequency sampling. Where local interval control is unavailable, evaluate gateway polling only as an explicitly labeled alternative.
- Use calibrated stage increasing with water elevation, not raw ultrasonic/radar distance (which decreases as stage rises), battery telemetry, temperature, or discharge unless separately configured. Stage percentage depends on gauge datum; reject nonpositive or near-zero baseline, invalid/out-of-range readings and stale or out-of-order samples. Add a configurable site-specific minimum absolute rise and optional absolute high-stage trigger, with thresholds reviewed per station before enabling. Do not fabricate a default threshold. For repeated bad readings, retain event state, flag data quality, and avoid falsely returning to normal. Keep an optional absolute high-water hold threshold for review.
- Distinguish normal sampling from event sampling and normal automatic reporting from Heltec's one-hour missing-AUTO fallback. No permanent new gateway polling slot is needed for a station that manages its own event cadence. Keep valid event-mode AUTO packets resetting the existing stale-sensor watchdog; no changes to R12's fallback threshold or scheduler until separately tested.
- Hourly local measurements can miss a short flood or detect it up to an hour late. Consider a separate low-power 15-minute local measurement/check cadence with hourly radio reporting in normal mode for flash-flood-prone sites. This alternative changes energy use and detection sensitivity and is not approved by the proposal alone. Check sensor startup/settling and power budget first.
- Persist the active event flag, fixed trigger and previous valid sample with bounded NVS writes on transitions; verify clock/reboot recovery, avoid repeated radio requests, and preserve real measurement timestamps during outages. Publish compact event start/end and current cadence metadata where backward compatible, with dashboard status and event shading. Store the full event measurements and clearly distinguish them from normal hourly readings; evaluate interval-aware graphs and alerts.
- Dashboard configuration should eventually expose per-station enable/disable, normal/event intervals, rise threshold, fixed exit ratio, absolute-rise guard and optional absolute high-stage hold/trigger. Default to disabled until calibrated and approved. Existing station firmware may not accept new remote parameters; verify protocol support before claiming remote application.
- Acceptance: simulate exactly 100%, more than 100%, slow rise, noisy near-zero data, invalid samples, falling stage and exact exit threshold; verify fixed trigger remains unchanged throughout the event, no spurious mode changes, 15-minute fresh measurements, hourly recovery, reboot persistence, offline operation, accepted telemetry and battery/radio load. One station running 15-minute reporting continuously produces 96 readings/day versus 24 normally (4× for that measurement stream), so assess total airtime and power under simultaneous events.

This is a future field-station feature, coordinated with gateway/dashboard support. Do not modify the currently running R12 firmware or deploy any station firmware as part of documenting this proposal.

## TLS allocation repair (following R9 field failure)

Build R10 did not publish: its allocator translation unit evaluated the gateway-enable macro before including the header that defines it. The corrected rebuild includes that header before the guard and still requires actual ESP32 SDK compilation/linking and the runtime PSRAM allocation probe. R11 then exposed an Arduino `EXTERNAL` macro collision. R12 uses distinct `TLS_PSRAM_CAPS` and `TLS_INTERNAL_CAPS` identifiers; allocation behavior is unchanged. R12 passed CI and was published; R10 and R11 never produced deployable artifacts.

On boot 58, the gateway retained 48–52 KB internal free heap and 36,852-byte largest free block during the outage, but the lifetime minimum fell to 204 bytes. Retained HTTPS records captured TLS error -17040 (RSA public operation plus MPI allocation failure) and repeated transport failures while LoRa remained responsive. Reboot restored HTTPS. This supports allocation pressure, but does not establish every cause of the wider outage or interrupt watchdog resets.

This V13 rebuild installs the pinned SDK's real Mbed TLS runtime calloc/free hook once at startup after PSRAM initialization. Allocations prefer external byte-addressable PSRAM; if necessary, internal fallback preserves a 24 KiB reserve on a best-effort basis. Matching capability-aware free handles both memory regions. The actual firmware build fails if the SDK does not expose the hook; at boot, a test allocation verifies the linked implementation invokes it. Health reports include tls_psram, tls_ps_allocs, tls_int_fallback and tls_alloc_fail. If PSRAM is unavailable, the allocator is not changed and the health flag remains zero. The boot probe is not proof of long-duration hardware stability.

The gateway now paces failed HTTPS transport independently per destination (15, 30, 60, 120 seconds maximum) with a shared five-second settling interval after each failed request. Deferred queue entries do not consume retries; legacy and Cloudflare queues remain independent. Successful transport clears that host's cooldown. Existing request cleanup and independent short-lived TLS sessions are retained: persistent keep-alive is deliberately deferred because it retains TLS memory and would change session lifetime/OTA ordering. No changes to LoRa, DM commands (including any-radio REBOOT and paged DIAG/CRASHLOG), station IDs, polling intervals, auto-DM stale clocks, storage, signed OTA loader or watchdog timing. Validate via heartbeat allocator counters, increasing uptime, accepted uploads and actual auto-DM delivery. The download links below follow the most recent successful CI artifact, not an unbuilt source revision.

## Radio diagnostic replies

Send `DIAG` directly to Heltec from another radio. The reply shows its page number and, when more detail is available, the next command (`DIAG 2`, then `DIAG 3`, and so on). `CRASHLOG` is an alias and accepts the same page number. Each command sends one reply; later pages read the current diagnostic snapshot. The command works independently of the internet connection.

Earlier V13 builds could send a 229-byte diagnostic text. With its protobuf (including the router's originator bitfield), radio header and PKI encryption, that reply occupies 264 bytes, beyond the 255-byte radio limit. This correction limits HOBO text replies to 200 bytes and divides the complete diagnostic report into requested pages. The previously installed R5 and the R8 artifact contain the oversized-reply bug. R12 retains this correction; actual field delivery still needs confirmation.

## Automatic TLS memory recording

The R9 diagnostic build and subsequent R12 record internal free RAM, largest internal allocation, lifetime minimum free RAM, and available PSRAM around each outbound HTTPS operation. Its fixed RTC buffer keeps six request records plus the first and most recent failures across software resets. Each record contains the boot number, operation, timing, HTTP result, TLS error and the errno observed when connect returned. The four memory samples are taken before connection, after handshake returns, after the HTTP call returns, and after client destruction. The lifetime low watermark also covers other running tasks and earlier requests.

Health reporting runs every five minutes for this investigation. The authenticated Cloudflare health handler stores the numeric trace in `mesh_gateway_tls_diag`. Sensor uploads also include a compact current internal-heap snapshot in their existing supervisor metadata. Apply `cloudflare/cces-mesh-remote/tls-memory-schema.sql` and deploy that Worker's matching health handler before installing this image. The 8 KiB health limit accommodates the auto-DM snapshot and the bounded memory trace. Missing or malformed optional traces leave normal health confirmation available.

The pinned Arduino-ESP32 2.0.17 source closes TLS sockets and frees SSL, configuration, RNG, certificate and key contexts through its failure and destruction paths. R5's field record included TLS error `-16`, the library's memory-allocation error, while approximately 50 KiB was free at recovery. Those measurements leave the allocation peak and its owner unresolved. The diagnostic rebuild measures the allocation stages. Host tests exercise 10,000 recorder cycles, a simulated allocation failure, destruction order, timer rollover, interrupted-operation retention, payload validation and authenticated storage. A physical ESP32-S3 reproduction and a sustained field run remain required to establish a hardware fix.

The trace's compact numeric rows use the following mapping.

| Index | Value |
| --- | --- |
| 0–3 | Retained sequence, boot counter, start uptime in milliseconds, duration in milliseconds |
| 4 | Operation. 1 config, 2 health, 3 Vercel sensor upload, 4 Vercel HEAD probe, 5 Cloudflare sensor upload, 6 OTA download |
| 5 | Stage. 1 started, 2 handshake returned, 3 HTTP returned, 4 cleaned up, 5 interrupted by reset |
| 6–8 | HTTP result, TLS result, observed connect errno |
| 9–12 | Before-connect free, largest, lifetime minimum and PSRAM bytes |
| 13–16 | The same four values after handshake returns |
| 17–20 | The same four values after the HTTP call returns |
| 21–24 | The same four values after client destruction |

Zero-filled later samples indicate a stage the operation has yet to reach. A health request can include its own in-progress row. The next successful report includes its completed row if still in the six-record history. A power cycle can clear RTC history.

## November 27 network recovery correction

On November 27, 2026, the gateway installed V13 R4 on boot 38. Cloudflare recorded software resets on boots 39–49. Forwarded packets showed retained recovery code 6 and the network reboot counter advancing. R4 used code 6 for Wi-Fi loss, missed configuration polls, and upload stalls. The stored evidence identifies the network recovery path and leaves the first link or service failure unresolved.

Two failed sensor POSTs could disconnect a working Wi-Fi link. R4 could then restart the ESP32 after one service went quiet, even while the other was reachable. Its heartbeat also labeled its own active HTTPS request as the latest event. The correction keeps station reconnects under one supervisor and reports the preceding boot's recorded event separately.

| What the gateway sees | What it does |
| --- | --- |
| A Cloudflare or Vercel endpoint replies, including a 401, 405 or 5xx | Treats that response as proof of connectivity. Only successful validation or ingestion counts toward the corresponding application counter. |
| Two Cloudflare config attempts fail at the transport level | Sends a credential-free HTTPS HEAD to the existing Vercel ingest URL, at most once every five minutes. This works even with no sensor traffic. |
| The station has no IPv4 address | Requests a Wi-Fi rejoin after two minutes and restarts the Wi-Fi driver after ten minutes if the link is still down. |
| The station has an IP but both destinations fail and all HTTP responses stop | Requests a rejoin after twenty minutes of silence and a Wi-Fi driver restart after thirty minutes. A reply from either destination clears the escalation. |
| A confirmed outage lasts an hour despite driver recovery | Allows one gateway reboot. Further attempts repair Wi-Fi at fifteen-minute intervals. A new network-triggered reboot requires ninety uninterrupted minutes of station IP and recent validated configuration. |

The 90-second task watchdog, guarded LoRa recovery, twelve-hour preventive reboot, and deliberate radio reboot remain available. Send `REBOOT` as a direct message from any radio using the supported PKI or LongFast control channel. Heltec acknowledges and schedules a restart twenty seconds later. Cloud authorization, signed OTA, the recovery loader and existing station settings retain their existing behavior.

## Station data and field verification

Each enabled missing-AUTO watch needs an uninterrupted hour without its expected sensor reading before it can issue a rescue DM at its configured clock boundary. Battery reports do not count as sensor readings. Rejoining Wi-Fi keeps the radio, missing-AUTO clocks and RAM queues running. A full reboot clears volatile state. Pack Creek, Mill Creek, Matheson, Cottonwood Relay and Fishlake retain their existing station policies. Vercel/Neon receives `heltec_dm` readings using the existing routes.

For an installation, verify the applied release ID in Cloudflare. Compare the boot number and uptime across at least two hours, check for new accepted sensor packets, and inspect an eligible station for an actual rescue DM. Cloudflare reset diagnostics and a radio `DIAG` show the previous boot's recorded event. The Cloudflare detail also retains the first and latest network repair, boot/uptime, IP-present flag, Wi-Fi disconnect code, HTTP/TLS results, RSSI, free heap and largest internal block. A transport value of zero can mean no attempt. The last abnormal reset has its own historical field.

[Gateway management](https://monitor.canyoncountryecological.com/#firmware) · [V12 release history](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v12/downloads/Heltec-Gateway-v12) · [V9 Clock before Cloudflare integration](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v9-hour-boundary)

[application-bin]: https://github.com/canyoncountryadventure/firmware/raw/refs/heads/Heltec-Gateway-v13/downloads/Heltec-Gateway-v13/Heltec-Gateway-v13-11-27-26-r12.bin
[companion-zip]: https://github.com/canyoncountryadventure/firmware/raw/refs/heads/Heltec-Gateway-v13/downloads/Heltec-Gateway-v13/Heltec-Gateway-v13-11-27-26-r12.zip
[build]: https://github.com/canyoncountryadventure/firmware/blob/Heltec-Gateway-v13/downloads/Heltec-Gateway-v13/BUILD.txt
[workflow]: https://github.com/canyoncountryadventure/firmware/actions/workflows/build_cca_heltec_gateway_v13.yml?query=branch%3AHeltec-Gateway-v13
