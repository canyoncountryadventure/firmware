# Heltec Gateway V13

Moab watershed gateway firmware for the **Heltec WiFi LoRa 32 V4 OLED, ESP32-S3**, PlatformIO target `heltec-v4`.

[Download the published application BIN][application-bin] · [Companion ZIP][companion-zip] · [SHA-256 and source commit][build] · [Build status][workflow]

These links follow the last successful V13 publish. The dated filenames and source commit in BUILD.txt identify exactly which revision you are downloading. [Flash and rollback notes](downloads/Heltec-Gateway-v13/README.md)


## TLS allocation repair (following R9 field failure)

Build R10 did not publish: its allocator translation unit evaluated the gateway-enable macro before including the header that defines it. The corrected rebuild includes that header before the guard and still requires actual ESP32 SDK compilation/linking and the runtime PSRAM allocation probe. R9 remains the last published release until the replacement passes CI.

On boot 58, the gateway retained 48–52 KB internal free heap and 36,852-byte largest free block during the outage, but the lifetime minimum fell to 204 bytes. Retained HTTPS records captured TLS error -17040 (RSA public operation plus MPI allocation failure) and repeated transport failures while LoRa remained responsive. Reboot restored HTTPS. This supports allocation pressure, but does not establish every cause of the wider outage or interrupt watchdog resets.

This V13 rebuild installs the pinned SDK's real Mbed TLS runtime calloc/free hook once at startup after PSRAM initialization. Allocations prefer external byte-addressable PSRAM; if necessary, internal fallback preserves a 24 KiB reserve on a best-effort basis. Matching capability-aware free handles both memory regions. The actual firmware build fails if the SDK does not expose the hook; at boot, a test allocation verifies the linked implementation invokes it. Health reports include tls_psram, tls_ps_allocs, tls_int_fallback and tls_alloc_fail. If PSRAM is unavailable, the allocator is not changed and the health flag remains zero. The boot probe is not proof of long-duration hardware stability.

The gateway now paces failed HTTPS transport independently per destination (15, 30, 60, 120 seconds maximum) with a shared five-second settling interval after each failed request. Deferred queue entries do not consume retries; legacy and Cloudflare queues remain independent. Successful transport clears that host's cooldown. Existing request cleanup and independent short-lived TLS sessions are retained: persistent keep-alive is deliberately deferred because it retains TLS memory and would change session lifetime/OTA ordering. No changes to LoRa, DM commands (including any-radio REBOOT and paged DIAG/CRASHLOG), station IDs, polling intervals, auto-DM stale clocks, storage, signed OTA loader or watchdog timing. Validate via heartbeat allocator counters, increasing uptime, accepted uploads and actual auto-DM delivery. The download links below follow the most recent successful CI artifact, not an unbuilt source revision.

## Radio diagnostic replies

Send `DIAG` directly to Heltec from another radio. The reply shows its page number and, when more detail is available, the next command (`DIAG 2`, then `DIAG 3`, and so on). `CRASHLOG` is an alias and accepts the same page number. Each command sends one reply; later pages read the current diagnostic snapshot. The command works independently of the internet connection.

Earlier V13 builds could send a 229-byte diagnostic text. With its protobuf (including the router's originator bitfield), radio header and PKI encryption, that reply occupies 264 bytes, beyond the 255-byte radio limit. This correction limits HOBO text replies to 200 bytes and divides the complete diagnostic report into requested pages. Both the installed R5 and the previous R8 artifact contain the oversized-reply bug. Confirm the newly published release ID in a device heartbeat before expecting this correction on the radio.

## Automatic TLS memory recording

The next V13 diagnostic rebuild records internal free RAM, largest internal allocation, lifetime minimum free RAM, and available PSRAM around each outbound HTTPS operation. Its fixed RTC buffer keeps six request records plus the first and most recent failures across software resets. Each record contains the boot number, operation, timing, HTTP result, TLS error and the errno observed when connect returned. The four memory samples are taken before connection, after handshake returns, after the HTTP call returns, and after client destruction. The lifetime low watermark also covers other running tasks and earlier requests.

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

[application-bin]: https://github.com/canyoncountryadventure/firmware/raw/refs/heads/Heltec-Gateway-v13/downloads/Heltec-Gateway-v13/Heltec-Gateway-v13-11-27-26-r9.bin
[companion-zip]: https://github.com/canyoncountryadventure/firmware/raw/refs/heads/Heltec-Gateway-v13/downloads/Heltec-Gateway-v13/Heltec-Gateway-v13-11-27-26-r9.zip
[build]: https://github.com/canyoncountryadventure/firmware/blob/Heltec-Gateway-v13/downloads/Heltec-Gateway-v13/BUILD.txt
[workflow]: https://github.com/canyoncountryadventure/firmware/actions/workflows/build_cca_heltec_gateway_v13.yml?query=branch%3AHeltec-Gateway-v13
