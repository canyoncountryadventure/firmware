# Heltec Gateway V13

Moab watershed gateway firmware for the **Heltec WiFi LoRa 32 V4 OLED, ESP32-S3**, PlatformIO target `heltec-v4`.

[Download the published application BIN][application-bin] · [Companion ZIP][companion-zip] · [SHA-256 and source commit][build] · [Build status][workflow]

These links follow the last successful V13 publish. The dated filenames and source commit in BUILD.txt identify exactly which revision you are downloading. [Flash and rollback notes](downloads/Heltec-Gateway-v13/README.md)

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

[application-bin]: https://github.com/canyoncountryadventure/firmware/raw/refs/heads/Heltec-Gateway-v13/downloads/Heltec-Gateway-v13/Heltec-Gateway-v13-11-27-26-r5.bin
[companion-zip]: https://github.com/canyoncountryadventure/firmware/raw/refs/heads/Heltec-Gateway-v13/downloads/Heltec-Gateway-v13/Heltec-Gateway-v13-11-27-26-r5.zip
[build]: https://github.com/canyoncountryadventure/firmware/blob/Heltec-Gateway-v13/downloads/Heltec-Gateway-v13/BUILD.txt
[workflow]: https://github.com/canyoncountryadventure/firmware/actions/workflows/build_cca_heltec_gateway_v13.yml?query=branch%3AHeltec-Gateway-v13
