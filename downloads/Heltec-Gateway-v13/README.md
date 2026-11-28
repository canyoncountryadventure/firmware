# Heltec Gateway V13 downloads

**Hardware** · Heltec WiFi LoRa 32 V4 OLED, ESP32-S3. **PlatformIO environment** · `heltec-v4`.

| File | Purpose |
| --- | --- |
| [Dated application BIN][application-bin] | Application-only image for the existing Wi-Fi OTA or Cloudflare recovery loader. |
| [Companion ZIP][companion-zip] | Companion build files and ELF for diagnostics. |
| [BUILD.txt][build] | SHA-256 for both files, exact source commit and build URL. |

The build updates these links after all tests and the ESP32-S3 build pass. Older dated files in this folder are historical releases. Match the filename and SHA-256 in BUILD.txt before use. PowerShell can compute the digest with `Get-FileHash -Algorithm SHA256 <path-to-bin>`.


## R12 field status — November 27, 2026 (MST)

**R12 is the installed, provisionally healthy V13 candidate.** Verified at 9:04:49 PM on boot 65, with OTA authorization automatically cleared. The 9:55 PM heartbeat confirmed 50m 40s uptime, 484,257 PSRAM allocation calls, zero internal fallbacks/failures, successful uploads and no new TLS failures. The minimum internal heap recorded this boot was 41,860 B, versus the earlier R9 low of 204 B. R10 failed linking and R11 failed compiling; neither was deployed. Reboot/rejoin and watchdog revisions alone did not resolve the HTTPS outage. The PSRAM allocator and retry pacing are the strongest repair evidence so far; overnight stability and live fallback DM delivery remain unverified.

[Full incident timeline, source/run links, exact diagnostic records, D1 table names and acceptance checklist](../../README.md#field-incident-record--november-27-2026-mst). Keep the running image unchanged until the endurance test is complete.

## TLS allocation repair (following R9 field failure)

Build R10 did not publish: its allocator translation unit evaluated the gateway-enable macro before including the header that defines it. The corrected rebuild includes that header before the guard and still requires actual ESP32 SDK compilation/linking and the runtime PSRAM allocation probe. R11 then exposed an Arduino `EXTERNAL` macro collision. R12 uses distinct `TLS_PSRAM_CAPS` and `TLS_INTERNAL_CAPS` identifiers; allocation behavior is unchanged. R12 passed CI and was published; R10 and R11 never produced deployable artifacts.

On boot 58, the gateway retained 48–52 KB internal free heap and 36,852-byte largest free block during the outage, but the lifetime minimum fell to 204 bytes. Retained HTTPS records captured TLS error -17040 (RSA public operation plus MPI allocation failure) and repeated transport failures while LoRa remained responsive. Reboot restored HTTPS. This supports allocation pressure, but does not establish every cause of the wider outage or interrupt watchdog resets.

This V13 rebuild installs the pinned SDK's real Mbed TLS runtime calloc/free hook once at startup after PSRAM initialization. Allocations prefer external byte-addressable PSRAM; if necessary, internal fallback preserves a 24 KiB reserve on a best-effort basis. Matching capability-aware free handles both memory regions. The actual firmware build fails if the SDK does not expose the hook; at boot, a test allocation verifies the linked implementation invokes it. Health reports include tls_psram, tls_ps_allocs, tls_int_fallback and tls_alloc_fail. If PSRAM is unavailable, the allocator is not changed and the health flag remains zero. The boot probe is not proof of long-duration hardware stability.

The gateway now paces failed HTTPS transport independently per destination (15, 30, 60, 120 seconds maximum) with a shared five-second settling interval after each failed request. Deferred queue entries do not consume retries; legacy and Cloudflare queues remain independent. Successful transport clears that host's cooldown. Existing request cleanup and independent short-lived TLS sessions are retained: persistent keep-alive is deliberately deferred because it retains TLS memory and would change session lifetime/OTA ordering. No changes to LoRa, DM commands (including any-radio REBOOT and paged DIAG/CRASHLOG), station IDs, polling intervals, auto-DM stale clocks, storage, signed OTA loader or watchdog timing. Validate via heartbeat allocator counters, increasing uptime, accepted uploads and actual auto-DM delivery. The download links below follow the most recent successful CI artifact, not an unbuilt source revision.

## November 27 correction

R9 and subsequent R12 fix silent `DIAG` replies: the old 229-byte text exceeds the encrypted radio packet limit. Send `DIAG` for page one and follow its `Next: DIAG 2` instruction for more detail. Each request returns one packet of at most 200 text bytes. `CRASHLOG` accepts the same page numbers. R5 and the previous R8 artifact retain the bug; the source commit in BUILD.txt identifies the correction after successful publication.

R9 and subsequent R12 include automatic TLS memory recording for config, health, both sensor destinations, the connectivity probe and OTA downloads. Health reports run every five minutes and carry a bounded RTC history to authenticated Cloudflare storage. The main README documents each field, the observed allocation error and the source audit. R12 has preliminary physical-device evidence; long-duration stability is still pending. Read the source commit in BUILD.txt to identify the exact installed image.

V13 R4 could disconnect Wi-Fi after two sensor upload failures and repeatedly restart the radio during a single-service outage. The correction tests Cloudflare and Vercel independently, repairs the Wi-Fi driver, and allows one network-triggered reboot per sustained outage. The gateway preserves LoRa and auto-DM operation during Wi-Fi repair. [Field evidence, recovery thresholds and operating notes](../../README.md)

## OTA and field acceptance

1. Use the application BIN and a unique Cloudflare release ID for each same-V13 rebuild. The loader remembers the applied ID. Preserve the installed recovery loader in OTA_1, station credentials and NVS.
2. Confirm the applied release ID and cleared OTA authorization on the next device heartbeat. A published artifact or queued manifest confirms only that step.
3. Watch for two hours of increasing uptime with the same boot number and accepted station uploads. At an eligible station's configured boundary, check for a successful `heltec_dm` reading. The scheduled twelve-hour reboot is separately labeled.
4. If connectivity fails again, read `Prev event` and `Last network repair` in the Cloudflare reset detail. A network incident records the boot number and time, link/IP state, disconnect reason, endpoint codes and available memory. A later heartbeat keeps the prior event visible.

[Build workflow][workflow] · [R4 archived BIN](Heltec-Gateway-v13-11-26-26-r4.bin) · [V12 rollback release](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v12/downloads/Heltec-Gateway-v12)

[application-bin]: https://github.com/canyoncountryadventure/firmware/raw/refs/heads/Heltec-Gateway-v13/downloads/Heltec-Gateway-v13/Heltec-Gateway-v13-11-27-26-r12.bin
[companion-zip]: https://github.com/canyoncountryadventure/firmware/raw/refs/heads/Heltec-Gateway-v13/downloads/Heltec-Gateway-v13/Heltec-Gateway-v13-11-27-26-r12.zip
[build]: https://github.com/canyoncountryadventure/firmware/blob/Heltec-Gateway-v13/downloads/Heltec-Gateway-v13/BUILD.txt
[workflow]: https://github.com/canyoncountryadventure/firmware/actions/workflows/build_cca_heltec_gateway_v13.yml?query=branch%3AHeltec-Gateway-v13
