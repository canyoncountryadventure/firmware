# HOBOconnect / OnsetSDK MX2203 Record

This file preserves the application-side evidence used to finalize the MX2203 temperature decoder so the conversion does not have to be rediscovered later.

## Source APKs

The Android packages supplied during the 2026-08-19 reverse-engineering session were:

```text
HOBOconnect-base.apk
HOBOconnect-arm64.apk
```

The APK binaries themselves are **not committed to this repository**.

HOBOconnect is a .NET Android application. Its managed assemblies are stored in the Android assembly store, including `libassembly-store.so`. The application assemblies were extracted and `OnsetSDK.dll` was inspected for the logger definitions and sensor conversion code.

## Model-to-sensor mapping found in OnsetSDK

The SDK maps the MX2200-family product IDs to two related temperature sensor implementations:

| Logger | Product ID | OnsetSDK sensor | Bit depth |
|---|---:|---|---:|
| MX2201 | `0x2201` | `TempSensor32` | 12 |
| MX2202 | `0x2202` | `TempSensor32` | 12 |
| **MX2203** | **`0x2203`** | **`TempSensor2F`** | **14** |
| MX2204 | `0x2204` | `TempSensor2F` | 14 |
| MX2205 | `0x2205` | `TempSensor2F` | 14 |

This explains why the MX2203 raw temperature value is approximately four times the equivalent MX2201 raw value: the same temperature transfer is represented at 14-bit rather than 12-bit resolution.

## OnsetSDK constants

For `TempSensor2F`, the SDK initializes:

```text
SensorBitSize = 14
CONST_A = 175.72
CONST_B = 2^14 = 16384
CONST_C = 46.85
```

The Celsius conversion performed by the SDK is:

```text
C = raw × CONST_A / CONST_B - CONST_C
```

Therefore the production MX2203 conversion is:

```text
C = raw × 175.72 / 16384 - 46.85
F = C × 9/5 + 32
```

Equivalent Fahrenheit-only form:

```text
F = raw × 0.01930517578125 - 52.33
```

The firmware intentionally implements the Celsius form first and then converts Celsius to Fahrenheit, matching the structure recovered from OnsetSDK.

## BLE response tied to this conversion

The physical MX2203 returned the following `NEWREAD64` response structure:

```text
01 01 0B 04 04 00 04 04 [TEMP32 big-endian] ...
```

Example:

```text
01 01 0B 04 04 00 04 04 00 00 1D 41 ...
```

The temperature raw value is therefore:

```text
0x00001D41 = 7489
```

OnsetSDK conversion:

```text
C = 7489 × 175.72 / 16384 - 46.85
F = 92.25 F (rounded to the HOBO display precision)
```

The corresponding HOBO export was 92.25 F.

## Physical validation

A 2026-08-19 hot-to-cold water-bath run matched serial raw readings to 40 consecutive HOBO-exported temperatures.

Representative results:

| MX2203 raw | OnsetSDK | HOBO export |
|---:|---:|---:|
| 7489 | 92.25 F | 92.25 F |
| 7177 | 86.22 F | 86.22 F |
| 6460 | 72.38 F | 72.38 F |
| 5089 | 45.91 F | 45.91 F |

This validation replaced the earlier temporary empirical calibration. The final production firmware uses the recovered OnsetSDK formula, not the regression.

## Why this file exists

Earlier MX2201 work also involved reverse engineering HOBOconnect, but the application-side evidence was not preserved cleanly enough in the repository. This file is deliberately explicit about:

1. which APKs were used,
2. which application assembly contained the relevant logic,
3. which OnsetSDK sensor class MX2203 uses,
4. the exact constants and equation, and
5. the physical-data validation.

If future MX2200-family support is added, start here before re-deriving a temperature conversion empirically.
