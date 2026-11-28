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
