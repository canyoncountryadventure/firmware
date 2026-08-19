# HOBO Universal Test Module

Temporary bench-test module for the Seeed XIAO nRF52840 Kit.

This folder is intentionally separate from the existing `MX2201Telemetry` and `MX2001Diagnostic` implementations. The production logger-specific source files are not modified by this test.

## Goal

Use one Seeed firmware image to:

- find an Onset HOBO logger without a hard-coded MAC address;
- connect to the common HOBO BLE command service;
- identify MX2201 vs MX2001 from the live `NEWREAD64` response;
- accept a direct Meshtastic text message of `READ`;
- return temperature for MX2201;
- return temperature and water level for MX2001.

## Common live read command

```text
01 01 08 04 04 00 00 00 00 00 00
```

## Auto-discovery

A BLE advertisement is considered a HOBO candidate if at least one of these is present:

1. Onset manufacturer data beginning `C5 00`;
2. the known 128-bit HOBO BLE service UUID;
3. a local BLE name containing `HOBO`, `MX2201`, or `MX2001`.

The MAC address is learned from the advertisement. It is not compiled into the firmware.

The public MX2001 advertisement format uses a 22-byte manufacturer payload beginning `C5 00`; this is used only as an MX2001 hint. The live sensor response is authoritative for final model identification.

## Model identification

### MX2201

Expected live response begins:

```text
01 01 07 04 04 00 04 04
```

Bytes 8-11 contain the big-endian temperature raw value. The existing bench-proven MX2201 conversion is used.

Expected Meshtastic reply:

```text
MX2201
Temp: 74.8 F
```

### MX2001

Expected live response is split across the two known notification fragments. Temperature is decoded from fragment 1 and water level is decoded as a big-endian float in meters from fragment 2.

Expected Meshtastic reply:

```text
MX2001
Level: 2.37 ft
Temp: 63.1 F
```

## Probe sequence

After connecting, the test module:

1. sends `INIT`;
2. tries `NEWREAD64` directly;
3. if that times out, tries the metadata initialization profile suggested by the BLE advertisement and retries `NEWREAD64`;
4. if that still times out, tries the other known metadata profile and retries;
5. rejects the candidate for 60 seconds if neither MX2201 nor MX2001 response is observed.

This lets the test determine the logger model from protocol behavior instead of requiring a preconfigured MAC or model.

## Build target

From the firmware repository root:

```powershell
pio run -e seeed_xiao_nrf52840_kit
```

To build and upload when the Seeed is connected:

```powershell
pio run -e seeed_xiao_nrf52840_kit -t upload
```

## Bench test

1. Close HOBOconnect or otherwise make sure the phone is not actively connected to the logger.
2. Power the Seeed and keep one HOBO logger nearby.
3. Watch the serial log for `HOBO UNIVERSAL CANDIDATE FOUND` followed by either `HOBO UNIVERSAL IDENTIFIED MX2201` or `HOBO UNIVERSAL IDENTIFIED MX2001`.
4. From another Meshtastic node/phone, send a direct message containing exactly `READ` to the Seeed node.
5. Confirm the returned value against HOBOconnect or a known reference.
6. Repeat with the other logger model without changing firmware.

## Scope of this temporary test

This branch intentionally tests discovery, model identification, BLE live reads, and the Meshtastic `READ` command only. Automatic production telemetry remains in the existing logger-specific implementations until the universal path is proven on both physical loggers.
