# CCA Heltec Gateway V9: opt-in missing-AUTO recovery

**Release:** November 4, 2026  
**Target:** Heltec WiFi LoRa 32 **V4 OLED**, ESP32-S3, PlatformIO `heltec-v4`  
**Release image:** `Heltec-Gateway-v9-11-04-26.bin` (standard **non-factory** application BIN)  
**Source commit:** `29b4d236942666497a36ffbf8dd86b9cc6f97d4f`  
**Firmware marker:** `v9-opt-in-fallback-2026-11-04`

V9 is an **internet-connected Meshtastic telemetry receiver**. It continues forwarding valid automatic (AUTO) sensor and device packets to Vercel/Neon. Its **fallback direct messages (DMs) are opt-in**: no monitored station is polled by Heltec automatically until the operator enables that station with a DM.

- [Verified build and tests](https://github.com/canyoncountryadventure/firmware/actions/runs/41971187447)
- [V9 application BIN](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Heltec-Gateway-v9-opt-in/downloads/Heltec-Gateway-v9/Heltec-Gateway-v9-11-04-26.bin)
- [V9 ZIP (companion flashing files)](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Heltec-Gateway-v9-opt-in/downloads/Heltec-Gateway-v9/Heltec-Gateway-v9-11-04-26.zip)
- [Release checksum and build provenance](./BUILD.txt)
- [V9 source branch](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v9-opt-in)

## 1. Install by Wi-Fi OTA from VS Code PowerShell

The previous Heltec Wi-Fi address is `192.168.1.147`. Substitute its current LAN IP if different. Run on the same network as the Heltec.

```powershell
$ErrorActionPreference = "Stop"
$ip = "192.168.1.147"
$dir = "C:\HeltecUpdate"
New-Item -ItemType Directory -Force -Path $dir | Out-Null
$fw = Join-Path $dir "Heltec-Gateway-v9-11-04-26.bin"

Invoke-WebRequest "https://raw.githubusercontent.com/canyoncountryadventure/firmware/Heltec-Gateway-v9-opt-in/downloads/Heltec-Gateway-v9/Heltec-Gateway-v9-11-04-26.bin" -OutFile $fw

$expected = "B9454D084003ACBB97ED5A7D3201C4544AAB5FCFAC7E26A31E9A6EB747069468"
$actual = (Get-FileHash $fw -Algorithm SHA256).Hash
if ($actual -ne $expected) { throw "Firmware hash mismatch; do not flash." }

py -m meshtastic --host $ip --ota-update $fw
if ($LASTEXITCODE -ne 0) { throw "Wi-Fi OTA failed. Review console output." }
```

**Do not** use a `*.factory.bin`, erase NVS, or reset Meshtastic identity, channel keys, Wi-Fi credentials, or the existing OTA loader. The checked standalone BIN is the normal application image. Keep the gateway powered and its Wi-Fi available during OTA.

If `py -m meshtastic` is missing, install/update Meshtastic's Python CLI using `py -m pip install --upgrade meshtastic`, then retry. A successful updater exit is not proof the new application is running: verify it after reconnect.

### Verify after OTA

```powershell
$ip = "192.168.1.147"
$g = Invoke-RestMethod "http://$ip/json/gateway"
$g.firmware_fix
$g.auto_fallback | Select-Object enabled,paused_mask,missing_minutes,default_minutes
$g.auto_fallback.watched |
  Select-Object node,kind,enabled,threshold_minutes,last_auto_ms,last_request_ms
```

Expected after the **first V9 boot**:

- `firmware_fix` is `v9-opt-in-fallback-2026-11-04`.
- `auto_fallback.enabled` is `false`; every entry in `auto_fallback.watched` is disabled.
- The default watched-sensor deadline is **60 minutes**.
- Normal inbound AUTO packets and authenticated HTTPS uploads continue. Check `last_http_status` (2xx), `uploaded_total`, and current monitoring readings.
- The `last_auto_ms` and `last_request_ms` values are **milliseconds since boot**, not wall-clock timestamps.

The gateway `/json/gateway` endpoint is read-only and works independently of Neon. If it is unreachable, first verify the gateway's IP, Wi-Fi, and current firmware. Do not infer a successful flash just because the OTA command exited.

## 2. V9 behavior

| Feature | Actual V9 behavior |
|---|---|
| Automatic DM polling on initial upgrade | **All stations OFF**, regardless of old V8 schedules |
| Enabling one station | Enables **only that station**; other stations remain unchanged |
| Default rescue deadline | **1 hour since missing AUTO** after enabling (each sensor stream independently) |
| Supported configurable deadlines | **15 minutes, 30 minutes, 1 hour, 24 hours** |
| When AUTO arrives | Refreshes that stream's freshness timer; cancels an unsent rescue for that stream |
| When a fallback DM succeeds | Uploads real measurement tagged `heltec_dm`; **does not** reset the AUTO freshness timer |
| No DM response | Up to **3 send attempts**, separated by approximately **30 seconds**; next rescue cycle must wait the configured deadline |
| Disable station or all | Stops **Heltec-initiated automatic rescue DMs** only |
| Sensor's own scheduled transmissions | **Not changed** by these gateway commands |
| Battery-only telemetry | Still uploaded; **never** used as proof of fresh sensor-stage/temperature data |
| Failed sensor / HOBO-unavailable report | Local diagnostic only; V9 does **not** send legacy `cc01` failure-event POSTs to Neon |
| Old Cottonwood cloud control | V9 no longer GETs/POSTs `/api/ingest?control=cc01` every two minutes |
| Manual `POLL` | Available as a **one-time** operator request, not a repeating poll schedule |
| Settings across a reboot | Operator-enabled stations and intervals persist in ESP32 preferences |

**Important first-upgrade behavior:** V9 performs a one-time migration using `v9_optin` in its preferences. It clears all inherited recurring polling slots and pauses all fallback stations, but **does not erase unrelated Meshtastic NVS settings**. Once a station is enabled through V9, its choice is persisted for later restarts; the one-time migration is not repeated on every boot.

### Independent sensor watchdogs

- **Pack Creek:** two separate watches: SEN0313 ultrasonic stage via `WATER READ`, and HOBO MX2001 stage + temperature via `READ`. A fresh ultrasonic packet **does not** hide a stale HOBO stream, or vice versa.
- **South Cottonwood:** MB7388 ultrasonic stage via `WATER READ`. **No HOBO request** is scheduled.
- **Mill Creek and Matheson:** MX2001 `READ` covers both stage and temperature together.
- **Supported temperature stations:** `READ` requests an appropriate logger/temperature response. Each field radio must actually implement the command; being present on the mesh does not guarantee it can answer.
- **Relays with battery-only telemetry** (such as Cottonwood Relay 2 and Spare RAK) are not on V9's sensor fallback list.
- Multiple eligible requests use a shared queue; they do not blast every station simultaneously. Valid DM replies preserve source node and packet identity for deduplication.

## 3. Direct-message commands

**Send these messages TO the Heltec** using Meshtastic. A reply confirms command acceptance; a LoRa send/ACK alone does not prove the remote sensor replied or that Neon saved a reading.

### Turn a station on, set deadline, or turn it off

```text
AUTO DM PACK CREEK 1HR
AUTO DM PACK CREEK 15MIN
AUTO DM PACK CREEK 30MIN
AUTO DM PACK CREEK 24HR
AUTO DM PACK CREEK OFF

AUTO DM MILL CREEK 1HR
AUTO DM MATHESON 1HR
AUTO DM SOUTH COTTONWOOD 1HR
AUTO DM COTTONWOOD RELAY 1HR
AUTO DM HIDDEN VALLEY 1HR
AUTO DM FISHLAKE HIGHTOP 1HR
AUTO DM SEED RELAY 1 1HR
AUTO DM THOUSAND LAKE MOUNTAIN 1HR
AUTO DM MINI ME 1HR
```

To inspect the enabled list or disable **all** fallback polling:

```text
AUTO DM STATUS
AUTO DM OFF
```

`AUTO DM <station> 1HR` starts a fresh one-hour countdown at the moment Heltec processes the command. It is a **missing-AUTO deadline**, not a demand for hourly sensor sampling or an unconditional hourly poll. The first rescue occurs only when AUTO for that stream stays absent for the deadline.

Allowed intervals **as written** are `15MIN`, `30MIN`, `1HR`, `24HR` (also `15M`, `30M`, `1H` and `24H`). The old V8 `AUTO DM ON`, `AUTO DM INTERVAL ...`, `AUTO <station> 15 MIN`, and recurring `POLL <station> 60 READ` syntax are **not V9 commands**. Numeric values like `45` or `60` alone are not valid V9 durations.

### One-time manual reads

```text
POLL PACK CREEK WATER
POLL PACK CREEK READ
POLL PACK CREEK BOTH
POLL MILL CREEK READ
POLL MATHESON READ
POLL SOUTH COTTONWOOD WATER
```

`POLL <station>` without a mode defaults to `READ`. `BOTH` queues `WATER READ` followed by `READ`; only use a mode the remote station supports. Manual requests can be used while all AUTO-DM fallbacks are OFF. The gateway acknowledges when a one-time job is **queued**, not when a sensor has successfully answered.

### Supported station names and node IDs

| Station | Node ID | Decimal ID | Auto-DM stream |
|---|---|---:|---|
| Pack Creek | `!742ecff5` | 1949224949 | Ultrasonic + HOBO, separately |
| Mill Creek | `!6459c5bf` | 1683604927 | MX2001 |
| South Cottonwood | `!65165646` | 1695962694 | Ultrasonic |
| Matheson Groundwater | `!77788479` | 2004386937 | MX2001 |
| Cottonwood Relay | `!9093c167` | 2425602407 | Temperature |
| Hidden Valley | `!b57d051f` | 3044869407 | Temperature |
| Fishlake Hightop | `!5e021e35` | 1577197109 | Temperature |
| Seed Relay 1 | `!26966f7a` | 647393146 | Temperature |
| Thousand Lake Mountain | `!9df66d7e` | 2650172798 | Temperature |
| Mini Me | `!fccdcc93` | 4241345683 | Temperature |

Known station names, decimal node IDs, `!xxxxxxxx` hex IDs, and `0x...` IDs are accepted by the command parser. The target **must** be a recognized watched-sensor node. Changing the station name in a DM does not automatically add an unrecognized node to the firmware.

## 4. Who can issue control DMs?

**V9 did NOT implement control from any arbitrary radio by default.** It keeps the `cc01AdminNode` value stored previously on the Heltec. When nonzero, commands from other sender IDs are ignored. The earlier cloud-control record used **`!c22d7fec` (decimal `3257761772`)** as its authorized sender.

**Caveat:** the firmware checks `if (cc01AdminNode && sender != cc01AdminNode)`. If the Heltec's own saved admin value is **zero** (for example on a fresh unit), V9 accepts control commands from any node that can address the gateway over the accepted Meshtastic DM channel. The old Neon record alone cannot prove what is saved on this specific Heltec. Verify from the intended controlling radio and keep the device's stored access state in mind.

PKI-encrypted direct messages are accepted on Meshtastic's decrypted local channel index 0; compatible LongFast direct messages can use channel index 1. Commands should be addressed to Heltec itself, not broadcast as channel chat. Unauthorized control DMs are ignored rather than forwarded to Neon.

## 5. Vercel/Neon rollout order

**1. Flash and verify V9 on the Heltec first.** Confirm `firmware_fix` and the all-OFF status. Verify an actual AUTO measurement reaches the monitoring dashboard. Test an authorized `AUTO DM ...` command on just one known station, then `OFF` if you do not want it scheduled.

**2. Only after V9 is operating:** review and deploy the Vercel cleanup PR [Meshtastic #4](https://github.com/canyoncountryadventure/Meshtastic/pull/4). The PR removes the old CC01 control route/UI and event writes, optimizes database queries, and changes certain hourly jobs. **The PR was staged, not deployed, at V9 release time.**

**3. Do not expect `gateway_poll_commands`, `cc01_gateway_control`, or historical `cc01_sensor_events` rows to disappear from Neon.** Those are separate legacy database objects. V9 does not need its old cloud polling endpoint, but this firmware update **does not delete tables or existing measurements**.

**4. No HTTP/CDN caching is part of this firmware.** The monitoring site's no-cache behavior and refresh buttons are controlled separately by Vercel code.

## 6. Troubleshooting

| Symptom | What to check |
|---|---|
| OTA reports success but dashboard still shows old behavior | Read `/json/gateway` and verify the exact V9 firmware marker; check the real Heltec LAN IP |
| `AUTO DM STATUS` shows no stations enabled | This is **expected after the first V9 boot**. Enable stations one by one |
| The DM command gets no reply | Sender may not match Heltec's stored authorized-admin ID; also check destination, PKI/LongFast channel, mesh reachability, and radio status |
| Heltec says unknown station | Use a name from the table or its exact `!xxxxxxxx`/decimal node ID |
| No fallback DM immediately after `AUTO DM PACK CREEK 1HR` | Correct behavior; the one-hour clock starts when enabled and only triggers if the specific AUTO stream stays absent |
| Fallback DM sent but no data on Neon | Check that the remote firmware implements `WATER READ` or `READ`, whether a valid response arrived, `/json/gateway` upload counters, and the gateway HTTPS status |
| Several sensor streams are missing | Requests are serialized; up to three attempts per job with 30-second reply timeouts |
| "HOBO unavailable" or timeout but no event in Neon | Deliberate V9 change. Failures stay local instead of creating cloud failure-event writes |
| Heltec is on V9 but old CC01 control settings remain in Neon | Expected until the Vercel cleanup and separate data-retention decisions; Heltec no longer polls them |
| V9 after reboot polls a station that was previously enabled | **Expected:** explicit V9 operator opt-ins persist across regular reboot. `AUTO DM OFF` disables everything |
| A station only emits device/battery data | That battery packet does **not** reset a watched water-level/temperature AUTO clock |

## 7. Rollback and limits

The prior verified firmware remains under [Heltec-Gateway-v8](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v8) in `downloads/Heltec-Gateway-v8`. A V8 rollback restores **V8's old cloud-control checks and older fallback command behavior**. If Vercel PR #4 has already removed the CC01 control endpoint, V8 may repeatedly receive errors from those old checks; restore a compatible backend before expecting that old feature to work. A rollback is **not** equivalent to simply turning V9's AUTO DM OFF.

The V9 flash does not prove field-side LoRa coverage, successful BLE connection to a remote HOBO, or receipt of a specific packet. Confirm those from a real station. The gateway's historical diagnostic field names (some still begin with `cc01`) reflect earlier software versions; they do **not** mean that cloud-based CC01 control or recurring Cottonwood polling has been re-enabled.

## Release verification

The published application BIN is **2,210,112 bytes**, with SHA-256:

```text
b9454d084003acbb97ed5a7d3201c4544aab5fcfac7e26a31e9a6eb747069468
```

The build workflow completed its compatibility tests, ESP32-S3 compilation, and release-publish job successfully. `BUILD.txt` is the release's source-of-truth for the exact compiled source commit, image filename, and hash.
