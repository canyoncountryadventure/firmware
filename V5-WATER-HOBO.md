# RAK Water Distance + HOBO V5

The [front-page README](README.md) is the current release guide, with verified field-radio UF2, application-only OTA ZIP and matched drone-flasher UF2 downloads for SEN0313 and MB7388.

The [October 15 final audit](docs/diagnostic-8/V5-FINAL-AUDIT.md) records all RAK water-distance versions, the successful MX2001 evidence, power/scan/timing decisions and limits of the root-cause finding. V5.1 preserves the successful nonblocking eight-second water warm-up and bounded handshake wake/HFCLK ownership. It adds a two-minute active 50% scan burst at boot/recovery and every five missing minutes, while retaining 10% passive scan outside those bursts. It also shortens the failed-handshake cooldown from five minutes to 30 seconds. Stale-`STATUS` recovery is bounded: a BLE-only reset at 15 minutes, one watchdog whole-node reset five minutes later if needed, and the existing staggered 12-hour maintenance reboot. A valid `STATUS` clears the one-reset guard.

V5.1 also moves automatic MX2001 stage/temperature from the custom `PRIVATE_APP` packet to standard `TELEMETRY_APP` environmental metrics (`temperature` °C plus `distance` as stage mm), the same normal telemetry transport used by battery/device readings. See the [V5.1 field note](docs/diagnostic-8/V5_1_HOBO_RECOVERY.md) for exact behavior and commands.

[Validation](docs/diagnostic-8/VALIDATION.md) · [Flashing and rollback](docs/diagnostic-8/FLASHING.md) · [Exact source provenance](docs/diagnostic-8/water-version-provenance.json).
