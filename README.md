# CCA RAK Water Distance + HOBO V6
**Release date: October 17, 2026 · Target: RAK4631 on RAK19007**

This is a new V6 firmware branch. The proven [V5 branch](https://github.com/canyoncountryadventure/firmware/tree/RAK-Water-Distance-HOBO-v5) and its published field firmware remain unchanged. V6 combines the distance sensor (SEN0313 or MB7388), HOBO MX2001/MX2201/MX2203 logger polling, Meshtastic transport and autonomous Scout/distance-radio updates. It does not require a separate firmware for each creek.

<!-- V6_RELEASE_START -->
### Verified October 17, 2026 firmware

Build **0c29518** ([source commit](https://github.com/canyoncountryadventure/firmware/commit/0c29518b5cfa5e1ea8e8ef474c04499c86ec6266)). Both field sensor flavors and both matching Scout flasher images passed [GitHub CI](https://github.com/canyoncountryadventure/firmware/actions/runs/39058628382), including application/OTA/UF2/embedded Scout verification.

| Sensor | Field station USB | Field station BLE | Separate drone flasher | Matching record |
| --- | --- | --- | --- | --- |
| SEN0313 | [Field UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v6/downloads/RAK-HOBO-v6/field-rak-hobo-distance-10-17-26-SEN0313.uf2) | [BLE OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v6/downloads/RAK-HOBO-v6/field-rak-hobo-distance-10-17-26-SEN0313-OTA.zip) | [Drone Scout UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v6/downloads/RAK-HOBO-v6/drone-rak-hobo-distance-10-17-26-SEN0313.uf2) | [Matched manifest](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v6/downloads/RAK-HOBO-v6/drone-rak-hobo-distance-10-17-26-SEN0313.uf2.txt) |
| MB7388 | [Field UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v6/downloads/RAK-HOBO-v6/field-rak-hobo-distance-10-17-26-MB7388.uf2) | [BLE OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v6/downloads/RAK-HOBO-v6/field-rak-hobo-distance-10-17-26-MB7388-OTA.zip) | [Drone Scout UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v6/downloads/RAK-HOBO-v6/drone-rak-hobo-distance-10-17-26-MB7388.uf2) | [Matched manifest](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v6/downloads/RAK-HOBO-v6/drone-rak-hobo-distance-10-17-26-MB7388.uf2.txt) |

[SHA-256 checksums](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v6/downloads/RAK-HOBO-v6/SHA256SUMS.txt) · [Build provenance](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v6/downloads/RAK-HOBO-v6/BUILD.txt)

**Field UF2 is for the deployed station. Drone UF2 is for the separate autonomous Scout/RAK4631.** Use the matched Scout image, not an older drone image. No erase or factory reset for application-only updates. Physical startup, reconnection and drone transfer still require field tests.
<!-- V6_RELEASE_END -->

## What changed from V5

- **MX2001 hundredths:** stage and temperature retain two decimal places over the mesh in versioned MX packet V2. Heltec Gateway V5 supports both this 21-byte format and existing V1 19-byte/tenths packets. Decoder/database display must retain the received precision; no invented digits if HOBO provides a lower-resolution value.
- **Configurable distance bursts:** 8-second nonblocking Pololu 2810 settling, sensor OFF between jobs, sample count adjustable 3–20 (default 10); up to 2× the requested sample count to obtain fresh valid frames. Samples are median-combined for WATER READ, scheduled transmissions, calibration and verification. Count is stored in V5-compatible CRC-verified A/B config.
- **Remote hub:** default remains normal mesh broadcast; optional persistent unicast destination for water and HOBO telemetry. HUB SET does **not** select a specific relay or force traffic through Hidden Valley.
- **Preferred first relay for ALL originating application traffic (V6 routing update):** `RELAY PREFER !NODEID` now influences locally originated water/HOBO telemetry, standard device telemetry, outbound text DMs, DM replies, and traceroute requests. The preference is applied once in the common router **before** ACK tracking, not inside a sensor module. Ordinary relayed traffic, mesh ACK/control and admin commands retain Meshtastic routing. `RELAY ONCE !NODEID` still affects just the next sensor telemetry packet (RAM-only), to avoid being consumed by its own confirmation reply. `RELAY AUTO` restores ordinary routing. The preference is a **next-hop hint, not a guaranteed source route**: a destination such as Heltec can still receive a packet directly if it hears the sender. No special firmware is required on the preferred relay or Heltec for this hint.
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
| `RELAY`, `RELAY SHOW` | Show first-hop relay preference and any one-time override (not a HUB destination) |
| `RELAY PREFER !1234ABCD` | **Persist** preferred first relay for all locally originated application transmissions, including water/HOBO, standard telemetry, DMs and traceroute |
| `RELAY ONCE !1234ABCD` | Prefer that relay for **only the next water/HOBO sensor packet** (broadcast or HUB unicast); then return to saved preference |
| `RELAY AUTO`, `RELAY CLEAR` | Restore standard Meshtastic routing for both broadcast and unicast application packets; clear pending ONCE |
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
- Preferred first-hop routing is **both unicast and broadcast**, defaults to AUTO, and persists across RAK application-only updates. `HUB SET` remains a *telemetry destination*, independent of which intermediate relay is preferred. Incoming DMs are controlled by their originating radio, not by this station's outgoing preference. This does not override forwarded packets or mesh ACK/control packets. It is a preference rather than a guarantee that the destination will not hear the packet directly.
- **Fallback safety:** for *broadcasts*, up to four selected packets await evidence of the requested relay; if not overheard after **25 seconds**, the RAK sends a **new packet ID** as an unrestricted broadcast, avoiding mesh duplicate suppression. This can produce duplicate readings in the receiver database. If no fallback memory is available, it floods on the first try. For *unicast* text DMs, traceroute and HUB-targeted sensor packets requesting ACK, Meshtastic's existing bounded retransmissions apply. The **last retry floods normally without the preferred first hop**; no new packet ID is needed for that route-recovery mechanism. Neither mechanism guarantees end-to-end delivery; test under real RAK sleep and power behavior.
- First-hop routing uses Meshtastic's **last-byte node identifier**, so do not choose a relay sharing the same last byte with another nearby mesh node. Confirm using `RELAY SHOW` and the actual Hidden Valley node ID. Fall back to `RELAY AUTO` if troubleshooting.

- Battery voltage history is RAM-only and starts with the current boot. The charger/thermistor hardware, not this firmware, determines cold-charge protection.

## Test the preferred relay (Cottonwood/Pack Creek → Hidden Valley → Heltec)

1. From an ordinary working mesh, DM the RAK `RELAY PREFER !b57d051f` to save Hidden Valley's preferred first hop, then `RELAY SHOW` to verify the saved ID.
2. With your phone connected **to that RAK**, run the **ordinary Meshtastic traceroute to Heltec**. It now originates with the preferred next-hop hint. Traceroute remains a unicast probe, and its actual path may still be direct if Heltec hears the RAK.
3. For real sensor traffic, send `WATER TELEMETRY NOW` and inspect packets at Heltec. `next_hop: 31` (0x1F) proves the RAK selected Hidden Valley; `relay_node: 31` with a decremented hop count on the *same packet ID* indicates Hidden Valley actually forwarded it. A packet with sender's own last byte as `relay_node` is a **direct reception**, not relay proof.
4. For a single-water-packet test without changing persistent routing, use `RELAY ONCE !b57d051f` followed by `WATER TELEMETRY NOW`. Revert everything using `RELAY AUTO`.

**Traceroute limitations:** this is **not** a new `RELAY TRACE` DM command. It makes *normal outgoing traceroutes* use the saved first-hop hint. Standard traceroute records forwarding hops actually traversed but not every radio that hears the packet. When Heltec is directly reachable, it may receive and answer the probe without Hidden Valley appearing in the trace. A traceroute route is not proof of the water measurement's route. No physical multi-hop field test is claimed by CI.

## Release checks and rollback

Published V6 field/Scout links are inserted **only after** GitHub Actions proves both actual UF2 builds, OTA ZIP consistency, Scout embedded payload verification and checksums. A passing CI build does **not** prove a deployed logger will reconnect, a sensor is wired correctly, or airborne DFU will succeed. Test representative SEN0313, MB7388 and MX2001 stations before mass redeployment. If V6 fails, the [V5 firmware and its separate old download files](https://github.com/canyoncountryadventure/firmware/tree/RAK-Water-Distance-HOBO-v5) remain available.

See the [Diagnostic 8 historical investigation](https://github.com/canyoncountryadventure/firmware/tree/RAK-Water-Distance-HOBO-v5/docs/diagnostic-8), the [Heltec Gateway V5 companion branch](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v5), and the [V6 CI workflow](.github/workflows/build_rak_water_v5.yml).
