# RAK Water Distance + HOBO V8 — Three Ultrasonic Sensors

**Status: V8 source development and integration build; DYP hardware bench validation pending.**  
**Target hardware:** RAK4631 + RAK19007. **Base:** [final working V7](https://github.com/canyoncountryadventure/firmware/tree/RAK-Water-Distance-HOBO-v7).

V8 retains the proven V7 Meshtastic, HOBO, calibration, recovery, sensor sampling, normal routing and power management logic, and adds the DYP-A08 **UART Controlled** ultrasonic driver. **V7 remains the final established working image for the two original deployed stations.** Do not replace those stations just to test the new driver.

## Supported three-sensor field configuration

| Sensor | Firmware DM selection | Interface | Power strategy |
| --- | --- | --- | --- |
| DFRobot SEN0313 / A01NYUB | `WATER SENSOR SEN0313` | Auto-streaming UART 9600 (4-byte binary) | Existing Pololu 2810, IO1-controlled eight-second warm-up, OFF between jobs |
| MaxBotix MB7388 | `WATER SENSOR MB7388` | Auto-streaming UART 9600 (ASCII R####\\r) | Existing Pololu 2810, IO1-controlled eight-second warm-up, OFF between jobs |
| **DYP-A08ANYTB-V1.0** (tentative exact ordered part) | **`WATER SENSOR DYP-A08`** | **UART Controlled 9600**, falling-edge trigger, 4-byte binary | **Direct regulated 3.3 V; no Pololu needed**, sensor idles between triggers |

The existing SEN0311/A02YYUW, SEN0590 and generic UART driver paths remain available from the V7 source, but the table lists the **three field sensors in this release's scope**, not every legacy driver compiled in.

The new DYP sensor's marking `A0811AT67U0141` is not proof of its output-mode suffix. **Confirm the sensor actually is UART Controlled (ANYTB) rather than UART Auto (ANYUB) before installation.**

## DYP-A08 controlled wiring and protocol

On the **RAK19007** (with the RAK4631 seated), connect DYP VCC to the regulated 3.3 V supply and DYP GND to common ground. Connect **sensor TX to RAK RX1**, and **sensor RX trigger input to RAK TX1**. The four conductors on the currently discussed unit are red, black, white and yellow respectively **only if the seller confirms the color-to-function assignment**. Do not assume a cable color guarantees its function. This setup bypasses the Pololu entirely on the new stations.

Per [DYP A08 output interface specification](https://www.dypcn.com/uploads/A08-Output-Interfaces.pdf), the trigger is a falling-edge low pulse approximately 10 microseconds to 2 milliseconds wide; a new result is returned on TX at 9600 baud with the bytes `0xFF, distance_hi, distance_lo, checksum`. The checksum is the low eight bits of their sum. V8 sends a single `0x00` byte on RAK TX1 at 9600 baud, producing **one approximately 0.94-millisecond LOW pulse** after the falling start edge and ending at the STOP bit. The response parser checks frame alignment, checksum, and the A08A range of 250–8,000 mm. Consecutive DYP triggers are separated by at least 150 ms (normally 200 ms) to protect the manufacturer's minimum cycle time.

The original V7 sensors retain their continuous-stream readers and Pololu power cycling. **DYP must remain connected to the RAK 3.3 V rail and does not switch off between samples.** IO1 stays low for the DYP mode. V8 does not attempt to auto-identify the DYP protocol from random UART traffic: select it explicitly per station.

## Configure the two new ultrasonic test stations

For each new RAK station, connect to Meshtastic and DM its own node:

```text
WATER SENSOR DYP-A08
WATER INTERVAL 15M
WATER SAMPLES 5
WATER SENSOR
WATER READ
WATER STATUS
VERSION
```

Selection, interval and sample count are saved in the existing CRC-protected A/B configuration slots. An application-only update retains Meshtastic identity/channels and calibration, assuming the prior stored configuration remains valid; changing sensor model while calibration is locked requires the existing explicit unlock/reset step. For the DYP test, **five valid readings** are collected per 15-minute report; the median is transmitted and sample spread is available in the existing diagnostic DMs. The firmware's unchanged default interval and sample count are **1 hour and 10**, respectively, so send the commands above on each new station.

The new mode sends its measurements in the **existing V7 water telemetry payload** so a receiving Heltec can keep parsing the same format. Separate Cloudflare/Neon routing is a *gateway change*, not built into the sensor RAK V8.

## Validation and release handling

- Build the unified V8 target: `rak4631_water_unified_v8` with GitHub Actions workflow `build_rak_water_v8.yml`.
- Confirm the downloaded UF2 and OTA ZIP are from the same passing build. Only flash the **field** image onto the sensor RAK. Preserve the existing working V7 binary as rollback.
- Run bench tests with the **actual** DYP-A08 sensor before unattended deployment: five triggers per burst, 15-minute timing, valid distances, bad checksums, no-return timeouts, reboots, power draw, and overnight reception. Source checks or successful compilation are **not** a substitute for this.
- Use the same sensor-side firmware on both new DYP test radios, with separate radio identities; do not flash either original SEN0313/MB7388 station just to join the test.

---

## V6 historical development notes (retained for lineage)

# CCA RAK Water Distance + HOBO V6
**Release date: October 17, 2026 · Target: RAK4631 on RAK19007**

This is a new V6 firmware branch. The proven [V5 branch](https://github.com/canyoncountryadventure/firmware/tree/RAK-Water-Distance-HOBO-v5) and its published field firmware remain unchanged. V6 combines the distance sensor (SEN0313 or MB7388), HOBO MX2001/MX2201/MX2203 logger polling, Meshtastic transport and autonomous Scout/distance-radio updates. It does not require a separate firmware for each creek.

<!-- V6_RELEASE_START -->
### Verified October 17, 2026 firmware

Build **4ea6cca** ([source commit](https://github.com/canyoncountryadventure/firmware/commit/4ea6cca31ab657b967bff285f43751bd34c5998d)). Both field sensor flavors and both matching Scout flasher images passed [GitHub CI](https://github.com/canyoncountryadventure/firmware/actions/runs/39071539897), including application/OTA/UF2/embedded Scout verification.

| Sensor | Field station USB | Field station BLE | Separate drone flasher | Matching record |
| --- | --- | --- | --- | --- |
| SEN0313 | [Field UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v6/downloads/RAK-HOBO-v6/field-rak-hobo-distance-10-17-26-4ea6cca-SEN0313.uf2) | [BLE OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v6/downloads/RAK-HOBO-v6/field-rak-hobo-distance-10-17-26-4ea6cca-SEN0313-OTA.zip) | [Drone Scout UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v6/downloads/RAK-HOBO-v6/drone-rak-hobo-distance-10-17-26-4ea6cca-SEN0313.uf2) | [Matched manifest](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v6/downloads/RAK-HOBO-v6/drone-rak-hobo-distance-10-17-26-4ea6cca-SEN0313.uf2.txt) |
| MB7388 | [Field UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v6/downloads/RAK-HOBO-v6/field-rak-hobo-distance-10-17-26-4ea6cca-MB7388.uf2) | [BLE OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v6/downloads/RAK-HOBO-v6/field-rak-hobo-distance-10-17-26-4ea6cca-MB7388-OTA.zip) | [Drone Scout UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v6/downloads/RAK-HOBO-v6/drone-rak-hobo-distance-10-17-26-4ea6cca-MB7388.uf2) | [Matched manifest](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v6/downloads/RAK-HOBO-v6/drone-rak-hobo-distance-10-17-26-4ea6cca-MB7388.uf2.txt) |

[SHA-256 checksums](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v6/downloads/RAK-HOBO-v6/SHA256SUMS.txt) · [Build provenance](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v6/downloads/RAK-HOBO-v6/BUILD.txt)

**Field UF2 is for the deployed station. Drone UF2 is for the separate autonomous Scout/RAK4631.** Use the matched Scout image, not an older drone image. No erase or factory reset for application-only updates. Physical startup, reconnection and drone transfer still require field tests.
<!-- V6_RELEASE_END -->

## What changed from V5

- **MX2001 hundredths:** stage and temperature retain two decimal places over the mesh in versioned MX packet V2. Heltec Gateway V5 supports both this 21-byte format and existing V1 19-byte/tenths packets. Decoder/database display must retain the received precision; no invented digits if HOBO provides a lower-resolution value.
- **Configurable distance bursts:** 8-second nonblocking Pololu 2810 settling, sensor OFF between jobs, sample count adjustable 3–20 (default 10); up to 2× the requested sample count to obtain fresh valid frames. Samples are median-combined for WATER READ, scheduled transmissions, calibration and verification. Count is stored in V5-compatible CRC-verified A/B config.
- **Remote hub:** default remains normal mesh broadcast; optional persistent unicast destination for water and HOBO telemetry. `HUB SET` changes the destination only; forwarding still follows normal Meshtastic routing.
- **V5 mesh routing restored:** V6 now uses the exact V5 `ReliableRouter` and `NextHopRouter` sources. Automatic and manual telemetry, DMs, replies, and traceroutes follow standard Meshtastic flooding/next-hop selection, ACK retries, and forwarding. All `RELAY` preference commands and the custom 25-second/new-ID fallback have been removed. Previously saved preferred-relay data is ignored, without erasing the rest of the radio's configuration.
- **Useful DM diagnostics:** retained watchdog/reset breadcrumbs and live radio/BLE/power statistics arrive in two DM replies, not merely a serial-log statement. Limited battery voltage history is available since the current boot.
- **Recovery commands:** independently restart the water-sensor driver or HOBO BLE central while preserving calibration/locks; retain CPU soft reboot and hardware-watchdog reset; one-step LAST RESORT emergency reset.
- **Preservation:** ordinary firmware UF2/OTA and recovery preserve the existing radio identity, channel credentials, logger assignment, water calibration and sensor selection. Only explicit destructive commands clear those.

## Firmware choices

| Station sensor | Build target | Station flash | Scout flash |
| --- | --- | --- | --- |
| SEN0313 / A01NYUB | `rak4631_sen0313_v5` (inherited PlatformIO target) | Dated `field-...-SEN0313.uf2` | Same-date `drone-...-SEN0313.uf2` on a **separate Scout radio** |
| MB7388 | `rak4631_mb7388_v5` (inherited PlatformIO target) | Dated `field-...-MB7388.uf2` | Same-date `drone-...-MB7388.uf2` on a **separate Scout radio** |

Build-target suffix `v5` is inherited for compatibility with existing board definitions; the **V6 source branch and VERSION DM identify the actual release**. Field and Scout images are not interchangeable. Field USB uses station UF2 on mounted RAK drive; BLE update uses matching OTA ZIP; autonomous drone uses the matched Scout UF2 programmed into its own RAK radio. Verify file date, sensor and SHA-256 against BUILD.txt.

## DM command reference

Commands are sent as a **direct message to the field node** on its working Meshtastic channel. Commands are case-insensitive, with an optional slash where supported. These are operational names, not speculative commands.

| DM | Actual behavior |
| --- | --- |
| `HELP`, `VERSION` | Command summary; V6 build identification |
| `LOGGER`, `READ` | HOBO model/MAC/BLE status or fresh HOBO measurement |
| `LOCK`, `LOCK AA:BB:CC:DD:EE:FF`, `UNLOCK` | Retain target HOBO across reboot, or resume discovery |
| `HOBO RESTART` | Restart HOBO BLE recovery state machine without full-radio reboot |
| `WATER HELP`, `WATER STATUS`, `WATER READ`, `WATER RAW` | Water command guide, health, median measurement or raw sensor value |
| `WATER SENSOR`, `WATER SENSOR MB7388`, `WATER SENSOR SEN0313` | Check or persist sensor selection; changing sensor can invalidate calibration |
| `SENSOR RESTART`, `WATER SENSOR RESTART` | Reinitialize same driver without clearing calibration |
| `SAMPLES`, `SAMPLES 10`, `WATER SAMPLES 10` | Check or persist required 3–20 valid fresh readings, default 10 |
| `WATER INTERVAL 1H` | Persist water reporting interval |
| `WATER CAL STATUS`, `WATER CAL STAGE 1.42FT` | Inspect or perform reference calibration; requires a fresh sensor burst |
| `WATER VERIFY`, `WATER CAL LOCK`, `WATER CAL UNLOCK CONFIRM` | Verify and manage calibration lock |
| `WATER CAL RESET CONFIRM` | Clear calibration only |
| `WATER RESET WATER CONFIRM` | Restore water settings; **does not** erase Meshtastic identity/channels/keys |
| `HUB`, `HUB SHOW` | Show current telemetry destination; initial setting is broadcast |
| `HUB SET !1234ABCD` | Persist node-ID destination for automatic water and HOBO telemetry; no periodic re-entry needed |
| `HUB AUTO`, `HUB CLEAR` | Persist normal broadcast mode |
| `BATTERY`, `BATTERY HISTORY` | Live voltage/percentage/charging; hourly sampled voltage minimum/maximum/first/last this boot |
| `DIAG`, `STATS`, `WATCHDOG` | Two DM diagnostic messages; radio stats; field/core watchdog state |
| `CLEAR DIAG` | Clear retained failure breadcrumb; boot count kept |
| `REBOOT`, `RECOVER` | Scheduled independent watchdog-origin hardware reset; may take up to watchdog timeout |
| `SOFT REBOOT`, `RADIO RESET` | Flash-safe CPU reboot, preserving settings; RADIO RESET is not a radio-only subsystem reset |
| `LAST RESORT` | **Immediately erases settings, bonds, logger target, calibration and mesh credentials**, provisions US + LongFast + frequency slot 20 + hop limit 7, then reboots. No second confirmation and no return DM required |

**LAST RESORT warning:** the old private channel/keys/identity disappear. It may not be reachable from the existing mesh until you re-pair or restore your intended mesh settings. It is an intentional destructive emergency command, never a substitute for REBOOT.

## Field operating choices

- Pololu 2810 MOSFET on RAK19007 IO1/P0.17; eight-second settling after powering the sensor, power OFF between measurements. MB7388 and SEN0313 do not use the same UART wiring/pin requirements; follow the station wiring documentation.
- HOBO central uses the proven automatic STATUS write-pointer polling (30 seconds), not a forced sensor record every 30 seconds. New logger records drive automatic telemetry.
- BLE startup has bounded CPU-awake/HFCLK protection, then releases it once logger STATUS baseline completes. Ordinary mesh relay works while other sensor jobs are pending.
- `HUB` remains broadcast on new or unconfigured stations; `HUB SET` survives normal reboot and application-only flashing. HUB is **not** a relay route override.

- Battery voltage history is RAM-only and starts with the current boot. The charger/thermistor hardware, not this firmware, determines cold-charge protection.

## Verify ordinary V5 mesh operation

1. Flash the new V6 build using the matching station UF2 or BLE OTA ZIP. Your existing channels, HOBO lock, water calibration and sensor settings should remain intact after application-only flashing.
2. Connect the Meshtastic app to the updated RAK, and use its **ordinary traceroute** to Heltec. No custom relay preference is applied. The route can be direct or multi-hop depending on the network, just as in V5.
3. DM `HUB AUTO` if you want water and HOBO measurements **broadcast** to the mesh, as in the default configuration. `HUB SET !NODEID` remains available for optional **destination unicast**, not relay selection.
4. DM `WATER TELEMETRY NOW`, inspect the resulting measurement at Heltec, and use `DIAG`, `LOGGER`, or `WATER STATUS` as needed. For MX2001 check the version-2 hundredths format in the gateway decoder.

**Removed deliberately:** `RELAY SHOW`, `RELAY PREFER`, `RELAY ONCE`, `RELAY AUTO`, and `RELAY CLEAR`. The old preferred-relay settings file is not read by this build. There is no need to factory-reset or clear your calibration to remove its effect. There is also no `RELAY TRACE` DM: use normal Meshtastic traceroute.

**Testing scope:** source-equivalence and CI compilation do not prove field radio paths, BLE pairing or solar power behavior. Test one nearby sensor radio before flashing the whole network.

## Release checks and rollback

Published V6 field/Scout links are inserted **only after** GitHub Actions proves both actual UF2 builds, OTA ZIP consistency, Scout embedded payload verification and checksums. A passing CI build does **not** prove a deployed logger will reconnect, a sensor is wired correctly, or airborne DFU will succeed. Test representative SEN0313, MB7388 and MX2001 stations before mass redeployment. If V6 fails, the [V5 firmware and its separate old download files](https://github.com/canyoncountryadventure/firmware/tree/RAK-Water-Distance-HOBO-v5) remain available.

See the [Diagnostic 8 historical investigation](https://github.com/canyoncountryadventure/firmware/tree/RAK-Water-Distance-HOBO-v5/docs/diagnostic-8), the [Heltec Gateway V5 companion branch](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v5), and the [V6 CI workflow](.github/workflows/build_rak_water_v5.yml).
