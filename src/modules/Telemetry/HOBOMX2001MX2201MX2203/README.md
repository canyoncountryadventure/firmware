# Universal HOBO Reader: MX2001 + MX2201 + MX2203

This is the production implementation used by branch `hobo-mx2001-mx2201-mx2203`.

## Supported logger data

- **MX2001:** live water level + temperature
- **MX2201:** live temperature
- **MX2203:** live temperature

## Meshtastic command

Send a direct message:

```text
READ
```

The bridge performs one fresh BLE `NEWREAD64` read and replies directly to the requester. There is no scheduled logger polling in this build.

## Shared Onset BLE protocol

Service UUID:

```text
CFCBE6BC-CC83-49AC-4146-4EED4F6EE165
```

Command characteristic:

```text
CFCBE6BC-CC83-49AC-4146-4EED4F6FE165
```

INIT:

```text
01 01 04 05 1C 01 00
```

NEWREAD64:

```text
01 01 08 04 04 00 00 00 00 00 00
```

## Live response identification

### MX2201

```text
01 01 07 04 04 00 04 04 [TEMP32 BE] ...
```

### MX2203

```text
01 01 0B 04 04 00 04 04 [TEMP32 BE] ...
```

MX2203 uses the OnsetSDK `TempSensor2F` 14-bit conversion documented in `ONSETSDK.md`.

### MX2001

The MX2001 response arrives in the two hardware-proven fragments used by the prior combined reader. Temperature is decoded from bytes 17-18 of fragment 1. Water level is the big-endian float beginning at byte 3 of fragment 2 and is converted from meters to feet.

## Discovery and fallback

The reader discovers Onset candidates dynamically from manufacturer data, the common HOBO service UUID, or a HOBO/MX local name. No logger MAC is hard-coded.

The first model probe is always:

1. `INIT`
2. direct `NEWREAD64`

This directly identifies MX2201 and MX2203 and normally identifies MX2001. For older MX2001/MX2201 behavior that requires metadata setup, the proven MX2001 and MX2201 metadata fallback commands are retained.

A positively identified MX2203 advertisement does not receive MX2001/MX2201 metadata fallback commands.

## Code provenance

- MX2001 + MX2201 behavior is based on the hardware-proven `hobo-mx2201-mx2001` reader.
- MX2203 response parsing is based on the hardware-proven `hobo-mx2203` reader.
- MX2203 temperature conversion comes from HOBOconnect `OnsetSDK.dll`, not a fitted field equation.

## Production naming

This folder is the active implementation. The older `HOBOMX2201MX2001` include path is retained only as a small compatibility router for the existing Meshtastic module registration.
