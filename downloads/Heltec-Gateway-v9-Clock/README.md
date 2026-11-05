# CCA Heltec Gateway V9 Clock — Verified Release

**Date:** November 4, 2026  
**Device:** Heltec WiFi LoRa 32 V4 OLED, ESP32-S3, PlatformIO `heltec-v4`  
**Image:** `Heltec-Gateway-v9-Clock-11-04-26.bin` (non-factory OTA application)  
**Compiled source:** `6a35d6c4e1528353f980988da45b192d76553012`  
**Firmware marker:** `v9-hour-boundary-2026-11-04`

- [Passed build, compatibility tests, and publication](https://github.com/canyoncountryadventure/firmware/actions/runs/41976737698)
- [OTA application BIN](Heltec-Gateway-v9-Clock-11-04-26.bin)
- [Release manifest and SHA-256](BUILD.txt)
- [V9 Clock source branch](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v9-hour-boundary)

## 1. VS Code PowerShell: flash over Wi-Fi

The previous Heltec LAN IP is `192.168.1.147`. Use its actual address if it changed. Keep the gateway powered and on the local network. **Never use a factory BIN or erase NVS for a routine OTA update.**

```powershell
$ErrorActionPreference = "Stop"
$ip = "192.168.1.147"
$dir = "C:\HeltecUpdate"
New-Item -ItemType Directory -Path $dir -Force | Out-Null
$fw = Join-Path $dir "Heltec-Gateway-v9-Clock-11-04-26.bin"
$url = "https://raw.githubusercontent.com/canyoncountryadventure/firmware/Heltec-Gateway-v9-hour-boundary/downloads/Heltec-Gateway-v9-Clock/Heltec-Gateway-v9-Clock-11-04-26.bin"
Invoke-WebRequest $url -OutFile $fw

$expected = "5B02183540FC16A8A8AF19FD2ED65FE34B0A6BBD96EE8724ED82DF5162CA1DF8"
if ((Get-FileHash $fw -Algorithm SHA256).Hash -ne $expected) {
  throw "Firmware checksum mismatch: DO NOT FLASH"
}
py -m meshtastic --host $ip --ota-update $fw
if ($LASTEXITCODE -ne 0) { throw "OTA failed; inspect updater output" }
```

If the Meshtastic CLI is missing, install it with `py -m pip install --upgrade meshtastic`.

**Verify the actual running firmware:**

```powershell
$g = Invoke-RestMethod "http://192.168.1.147/json/gateway"
$g.firmware_fix
$g.auto_fallback | ConvertTo-Json -Depth 6
```

Expected version: `v9-hour-boundary-2026-11-04`. Under `auto_fallback.watched`, the five requested stations are ON at `60` minutes, with Pack Creek's ultrasonic and HOBO independently watched. Other stations are OFF. Check a real automatic observation arrives at the monitoring server, not merely that OTA returned success.

## 2. Initial defaults and what changes

On its **first boot after upgrading to V9 Clock**, the firmware uses the one-time ESP32 preferences marker `v9clk_preset` to apply these settings:

| Station | Node ID | Initial fallback |
|---|---|---|
| Fishlake Hightop | `!5e021e35` | **ON, 1HR** |
| Mill Creek | `!6459c5bf` | **ON, 1HR** |
| Pack Creek | `!742ecff5` | **ON, 1HR**, separate ultrasonic and HOBO watches |
| Matheson Groundwater | `!77788479` | **ON, 1HR** |
| Cottonwood Relay | `!9093c167` | **ON, 1HR** |
| South Cottonwood | `!65165646` | **OFF** |
| Hidden Valley | `!b57d051f` | **OFF** |
| Seed Relay 1 | `!26966f7a` | **OFF** |
| Thousand Lake Mountain | `!9df66d7e` | **OFF** |
| Mini Me | `!fccdcc93` | **OFF** |

Battery-only relays without a supported sensor read command are not in the fallback list. Station settings changed later by DM persist across ordinary gateway reboots; the one-time preset should not overwrite them again.

### Clock-aligned scheduling

- A sensor must have **at least one full hour without a valid automatic sensor packet** before any automatic DM rescue is eligible. Device/battery telemetry does not reset the sensor timer.
- After that hour, Heltec waits for the **next clock-aligned boundary of the chosen interval**.
- At `1HR`, that means **2:00, 3:00, 4:00**, etc., never an arbitrary time measured from the last packet. Example: last AUTO at 12:45 PM → earliest 2:00 PM; last AUTO at 1:05 PM → earliest 3:00 PM.
- At `15MIN` and `30MIN`, the eligible boundaries occur at each quarter-hour or half-hour. The initial one-hour absence is **still mandatory**. `45MIN` boundaries are UTC 00:00, 00:45, 01:30, 02:15, etc. `6HR`/`12HR`/`24HR` use UTC midnight-anchored slots.
- The scheduler needs **synchronized time**, obtained from network/mesh RTC. With no trusted time it does not start unscheduled rescue traffic; normal AUTO reception and manual `POLL` remain functional.
- When several sensors are overdue, Heltec queues requests one at a time. Each can make up to three requests separated by roughly 30 seconds. DMs **may finish after :00**, but the automatic batch originates at the clock boundary.
- Valid automatic packets cancel pending unsent rescue requests for their respective sensor. A successful DM measurement is tagged `heltec_dm`, uploaded, and **does not reset the missing-AUTO timer**.
- Normal field-radio automatic telemetry, sensor logger sampling intervals, gateway Wi-Fi recovery, and watchdogs are not changed by these commands.

## 3. New DM commands: ANY sender radio

V9 Clock **removes the old single-radio authorized sender restriction**. Any Meshtastic radio that can deliver a correctly addressed, decoded PKI or supported LongFast direct message **to Heltec** can control the fallback settings. This includes other mesh users with compatible DM access; it is an intentional operator choice.

```text
AUTO DM STATUS
AUTO DM PACK CREEK 15MIN
AUTO DM PACK CREEK 30MIN
AUTO DM PACK CREEK 45MIN
AUTO DM PACK CREEK 1HR
AUTO DM PACK CREEK 6HR
AUTO DM PACK CREEK 12HR
AUTO DM PACK CREEK 24HR
AUTO DM PACK CREEK OFF

AUTO DM MILL CREEK 1HR
AUTO DM FISHLAKE HIGHTOP 1HR
AUTO DM MATHESON OFF
AUTO DM COTTONWOOD RELAY 1HR
AUTO DM SOUTH COTTONWOOD 1HR

AUTO DM OFF
```

A per-station OFF disables **only that station's fallback requests**. `AUTO DM OFF` disables **all** automatic fallback requests. Re-enabling a station starts a fresh one-hour missing-AUTO requirement; no instant flood of pending DMs. Manual reads remain possible with all automatic fallback disabled:

```text
POLL PACK CREEK WATER
POLL PACK CREEK READ
POLL PACK CREEK BOTH
POLL MILL CREEK READ
POLL MATHESON READ
```

Manual `POLL` replies are queued for real sensor read attempts, not guarantees of a successful observation. The remote radio must support the relevant `READ` or `WATER READ` command. Successful DM readings retain original packet identity and source node.

## 4. Neon and Vercel

V9 Clock forwards real accepted readings to `https://meshtastic-ecru.vercel.app/api/ingest`. Failed/unavailable sensor events remain **local diagnostics**, rather than waking Neon with repeated failure reports. The old CC01 cloud-control endpoint `/api/ingest?control=cc01` is not polled by the gateway.

The companion Vercel efficiency changes are **staged but not deployed** in [Meshtastic pull request #4](https://github.com/canyoncountryadventure/Meshtastic/pull/4). This includes optimized station-specific reads, hourly background checks, no dashboard caching, and updated command generation. Flash and verify the new Heltec firmware **before merging the web cleanup**. Flashing this firmware alone does not deploy code or delete any database table.

## 5. Troubleshooting and rollback

- **Wrong behavior after flashing:** use `/json/gateway` to confirm `v9-hour-boundary-2026-11-04`. Do not judge version by an old BIN left in the flash directory.
- **No rescue before top of hour:** expected. The minimum missing-AUTO age is 60 minutes, then the applicable clock boundary must arrive.
- **No rescue despite overdue station:** confirm `clock_synchronized` in diagnostics, the station is enabled, and its own node supports the relevant DM command.
- **No response to DM control:** verify it was sent as a direct message *to Heltec*, using a supported channel and correct station name/node ID. The sender is not restricted by the old admin setting.
- **No reply from the field logger:** LoRa transmission is not proof of a remote sensor response; examine gateway reply/upload counters and a real reading.
- **No Neon failure event:** intentional; failed sensor diagnostics stay local.
- **Rollback:** the original [V9 opt-in release](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v9-opt-in/downloads/Heltec-Gateway-v9) remains unchanged, but its defaults and radio authorization differ. V8 additionally depends on legacy CC01 cloud-control APIs, which the staged Vercel cleanup will remove.

## Provenance

GitHub Actions run: https://github.com/canyoncountryadventure/firmware/actions/runs/41976737698  
Source SHA: `6a35d6c4e1528353f980988da45b192d76553012`  
Size: **2,212,144 bytes**  
SHA-256 application BIN:

```text
5b02183540fc16a8a8af19fd2ed65fe34b0a6bbd96ee8724ed82df5162ca1df8
```
