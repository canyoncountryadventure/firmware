# CCA settings-safe firmware updates

## Rule

For an already configured CCA Seeed XIAO node, use the GitHub Actions artifact named:

```text
CCA-CCS3-SETTINGS-SAFE-OTA-<version>
```

The ZIP inside that artifact is a Nordic DFU **application-only** package. The build workflow validates `manifest.json` and fails the packaging job if any non-application DFU section is present.

## Do not use UF2 for routine upgrades

The `.uf2` file is retained for bootstrap/recovery use only. During the 2026-08-26 CCS3 update, copying the UF2 through the XIAO-SENSE bootloader resulted in Meshtastic settings reverting, including the owner/name and radio configuration. Therefore CCA routine upgrade instructions must not direct an already configured field node to the UF2 path.

Use UF2 only when:

- initially provisioning a blank/recovery device, or
- BLE/application DFU is unavailable and recovery is required.

If UF2 recovery is unavoidable, assume settings may need restoration afterward.

## PIR regression note

Firmware `CCA-MX-PIR-ROCK-1.0.4` removes the immediate LoRa rock-packet transmission that was introduced on every D6 PIR HIGH/LOW edge in commit `dc398892`.

The stable behavior is restored:

- D6 is still polled every 100 ms;
- LOW -> HIGH still increments the local rock motion counter;
- `CCAStationModule` retains the established PIR detection/DM-command behavior;
- rock telemetry is sent on its normal 60-second cadence rather than transmitting from the PIR edge handler;
- battery ADC remains on the safe 10-bit implementation and rock ADC is scaled to the established 0..4095 CCA calibration scale.

This avoids a possible self-trigger loop in which a LoRa transmission initiated by a PIR edge perturbs the PIR signal and causes another edge/transmission.
