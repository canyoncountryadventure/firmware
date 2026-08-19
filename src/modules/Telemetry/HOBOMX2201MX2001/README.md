# Compatibility Router Only

This directory is **not** the production implementation on branch `hobo-mx2001-mx2201-mx2203`.

`Modules.cpp` historically includes:

```text
modules/Telemetry/HOBOMX2201MX2001/HOBOMX2201MX2001Telemetry.h
```

To avoid an unnecessary change to unrelated Meshtastic module-registration code, that header is retained as a tiny compatibility router to:

```text
src/modules/Telemetry/HOBOMX2001MX2201MX2203/
```

The active three-model implementation supports:

- MX2001 — water level + temperature
- MX2201 — temperature
- MX2203 — temperature

Do not add logger logic to this compatibility directory.
