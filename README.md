> **Trail Sensors v2:** existing PIR/Rock/HOBO and SEN0171 sensor logic is retained on the Field-Recovery v2 radio/watchdog core.

# Trail Sensors v2

Consolidated Meshtastic trail-sensor development branch for **Seeed XIAO nRF52840 + Wio-SX1262**.

This branch preserves the three trail-sensor lines that were previously split across separate branches:

1. **PIR / presence station** — SEN0171-based CCA station logic.
2. **Rock telemetry** — CCA rock/remote-station telemetry paired with the PIR/HOBO station build.
3. **SEN0171 trail counter** — dedicated fast PIR event counter build.

Meshtastic base remains pinned to the validated **2.7.26** codebase.

## Build targets

### PIR + Rock + HOBO

PlatformIO environment:

```text
seeed_xiao_nrf52840_cca_mx_pir
```

This build retains:

- universal HOBO MX BLE telemetry with the v2 30-second connection timeout, bounded STATUS/NEWREAD recovery, and field-watchdog escalation
- SEN0171 PIR/presence handling
- RF-aware PIR suppression around local LoRa transmissions
- CCA battery/power diagnostics
- rock telemetry
- normal Meshtastic messaging/routing

The PIR input for this station build remains on **D6** and GPS is excluded to avoid the normal D6 GNSS pin conflict.

### SEN0171 trail counter

PlatformIO environment:

```text
seeed_xiao_nrf52840_trail
```

This is the dedicated trail-counter implementation. It uses **D0** for the SEN0171 PIR and suppresses the normal D0 GNSS standby assignment in this build.

Behavior:

- polls the PIR every 20 ms
- ignores startup HIGH until the sensor first returns LOW
- counts each distinct LOW → HIGH event
- records HIGH duration and LOW gap for testing
- transmits a Meshtastic detection message for each distinct event

The HOBO BLE module is intentionally not instantiated in the dedicated SEN0171 counter build so the branch can hold both implementations without forcing both sensor systems into the same firmware image. The PIR + Rock + HOBO image uses the canonical v2 HOBO state machine, but it intentionally does not instantiate the generic HOBO self-recovery command supervisor because `CCAStationModule` already owns overlapping station commands such as `STATUS`, `POWER`, and `VERSION`.

## Useful DM commands

### PIR + Rock + HOBO build

| Command | What it does |
|---|---|
| `VERSION` | Reports the CCA station firmware/version and Meshtastic base. |
| `STATUS` | Shows uptime, battery, charging state, PIR state/counts, alert destination, and HOBO hint. |
| `ALERTS HERE` | Saves the radio sending the command as the private destination for PIR/power/boot alerts. |
| `ALERTS STATUS` | Shows the currently saved private alert destination. |
| `ALERTS CLEAR` | Clears the private alert destination; only the current destination can clear it. |
| `PIR` or `PIR STATUS` | Shows PIR enabled state, live motion state, counts, last detection, and TX-alert state. |
| `PIR COUNT` | Shows cumulative and since-boot PIR detections. |
| `PIR LAST` | Shows how long ago the last PIR detection occurred. |
| `PIR RESET` | Resets the stored PIR counts to zero. |
| `PIR ON` | Enables PIR monitoring and saves the setting. |
| `PIR OFF` | Disables PIR monitoring and saves the setting. |
| `PIR TX ON` | Enables private PIR detection alerts. |
| `PIR TX OFF` | Stops PIR alert transmissions while continuing to count locally. |
| `PING` | Confirms the CCA application is alive and reports uptime. |
| `WATCHDOG` | Reports the 90-second main watchdog and independent field-health watchdog channel. |
| `RECOVER` or `REBOOT` | Schedules the v2 flash-safe whole-node reboot path in 2 seconds. |
| `POWER` or `POWER STATUS` | Shows battery voltage/percent, charging state, trend, min/max, and sample count. |
| `POWER VOLTAGE` | Shows current battery voltage, percentage, and charging state. |
| `POWER MINMAX` | Shows minimum, maximum, and current battery voltage since boot. |
| `POWER TREND` | Shows current voltage plus approximate 1 h, 6 h, and 24 h trend references. |
| `POWER HISTORY` | Shows the stored battery-history checkpoints and min/max. |
| `POWER RESET` | Clears battery-history statistics and starts them again from the current voltage. |
| `UPTIME` | Shows node uptime and boot count. |
| `BOOT` | Shows boot count and firmware version. |
| `LOGGER` | Shows the connected/discovered HOBO logger identity and state. |
| `READ` | Requests a fresh HOBO reading without consuming the automatic record pointer. |
| `LOCK` | Saves the identified HOBO logger as the station's assigned logger. |
| `UNLOCK` | Clears the saved HOBO assignment and resumes discovery. |
| `DEBUG ON` | Enables verbose CCA serial diagnostics until reboot. |
| `DEBUG OFF` | Disables verbose CCA serial diagnostics. |

### Dedicated SEN0171 trail-counter build

The dedicated `seeed_xiao_nrf52840_trail` build currently has **no custom plain-text DM command parser**. It automatically counts distinct PIR events and sends `PERSON WALKED BY #...` messages. Normal Meshtastic messaging/admin functions still work, but commands such as `PIR STATUS` above belong to the PIR + Rock + HOBO build, not the dedicated counter build.

## Key source files

```text
src/modules/CCAStationModule.cpp
src/modules/CCAStationModule.h
src/modules/CCARockTelemetryModule.cpp
src/modules/CCARockTelemetryModule.h
src/modules/TrailCounterModule.cpp
src/modules/TrailCounterModule.h
src/modules/Modules.cpp
variants/nrf52840/seeed_xiao_nrf52840_kit/platformio.ini
variants/nrf52840/seeed_xiao_nrf52840_kit/variant.h
```

## CI

The `Trail Sensors Build` workflow compiles both production targets on every push to this branch:

- PIR + Rock + HOBO
- SEN0171 trail counter

A successful compile verifies source integration, but field behavior still requires hardware validation before deployment.

## Repository organization

This branch exists so the old PIR-only, PIR+Rock, and SEN0171 branches can be retired without losing their active sensor implementations. Water-distance, HOBO-only safe firmware, and Heltec gateway firmware remain on their own canonical branches.
