# RAK Water Distance + HOBO V5

The [front-page README](README.md) is the current release guide, with verified field-radio UF2, application-only OTA ZIP and matched drone-flasher UF2 downloads for SEN0313 and MB7388.

The [October 15 final audit](docs/diagnostic-8/V5-FINAL-AUDIT.md) records all RAK water-distance versions, the successful MX2001 evidence, power/scan/timing decisions and limits of the root-cause finding. The selected package preserves the successful nonblocking eight-second water warm-up, passive 10% scan and bounded handshake wake/HFCLK ownership. It does not use the initial V5 50% scan, periodic scan restarts or automatic HOBO watchdog escalation described in earlier revisions of this file.

[Validation](docs/diagnostic-8/VALIDATION.md) · [Flashing and rollback](docs/diagnostic-8/FLASHING.md) · [Exact source provenance](docs/diagnostic-8/water-version-provenance.json).
