# HOBOconnect / OnsetSDK Temperature Conversion Record

This file permanently records the APK reverse-engineering result used by the MX2203 implementation so it does not need to be rediscovered.

## APKs inspected

Pulled from the installed Android package `com.onsetcomp.HOBOconnect`:

```text
HOBOconnect-base.apk
HOBOconnect-arm64.apk
```

The application is a .NET Android/MAUI build. Its managed assemblies were stored inside `libassembly-store.so`. The extracted SDK assembly was:

```text
OnsetSDK.dll
```

## Model mapping recovered from OnsetSDK

```text
MX2201 / MX2202 -> TempSensor32 -> 12-bit
MX2203 / MX2204 / MX2205 -> TempSensor2F -> 14-bit
```

Both temperature sensor classes use the same transfer constants:

```text
A = 175.72
C = 46.85
```

The difference is the sensor bit depth.

## MX2201 family

12-bit denominator:

```text
2^12 = 4096
```

Onset form:

```text
Temp C = raw × 175.72 / 4096 - 46.85
```

The universal branch intentionally preserves the previously hardware-proven MX2201 conversion used by the stable combined reader rather than changing the established MX2201 behavior during the three-model merge.

## MX2203 family

14-bit denominator:

```text
2^14 = 16384
```

Exact OnsetSDK conversion:

```text
Temp C = raw × 175.72 / 16384 - 46.85
Temp F = Temp C × 9/5 + 32
```

## Physical MX2203 validation

The formula was checked against the 2026-08-19 physical MX2203 hot-to-cold water-bath run and HOBO-exported data.

Examples:

| Raw | OnsetSDK | HOBO export |
|---:|---:|---:|
| 7489 | 92.25 F | 92.25 F |
| 7177 | 86.22 F | 86.22 F |
| 6460 | 72.38 F | 72.38 F |
| 5089 | 45.91 F | 45.91 F |

The conversion therefore comes from Onset's application code and independently matches the physical logger output.

## MX2203 protocol field

The hardware-proven MX2203 `NEWREAD64` response is:

```text
01 01 0B 04 04 00 04 04 [TEMP32 BE] 00 00 00 01 ...
```

The temperature raw value is the four-byte big-endian integer at bytes 8-11.

Do not replace this conversion with the earlier empirical calibration unless new evidence shows OnsetSDK behavior changed.
