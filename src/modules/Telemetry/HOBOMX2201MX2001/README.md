# Compatibility Router — `hobo-mx2203`

This folder is **not** the MX2203 implementation.

The upstream Seeed HOBO construction hook already includes `HOBOMX2201MX2001Telemetry.h`. To keep that proven integration point unchanged, the dedicated `hobo-mx2203` branch uses this header only as a thin compatibility router.

The production MX2203 code lives here:

```text
src/modules/Telemetry/HOBOMX2203/
```

See:

- [`../HOBOMX2203/README.md`](../HOBOMX2203/README.md) — final bridge behavior, protocol, build and flashing
- [`../HOBOMX2203/ONSETSDK.md`](../HOBOMX2203/ONSETSDK.md) — permanent HOBOconnect APK / OnsetSDK conversion record

The separate `hobo-mx2201-mx2001` branch remains unchanged and still contains the finalized combined MX2201/MX2001 reader.
