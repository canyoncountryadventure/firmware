# Trail Sensors

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

- universal HOBO MX BLE telemetry
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

The HOBO BLE module is intentionally not instantiated in the dedicated SEN0171 counter build so the branch can hold both implementations without forcing both sensor systems into the same firmware image.

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
