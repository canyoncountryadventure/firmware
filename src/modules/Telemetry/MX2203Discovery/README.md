# MX2203 Discovery Test

Temporary reverse-engineering branch for the HOBO TidbiT MX2203.

This work is intentionally isolated from the finalized `hobo-mx2201-mx2001` branch. Nothing here changes the proven MX2201/MX2001 production behavior.

## Branch

`mx2203-discovery-test`

Base branch: `hobo-mx2201-mx2001`

## Official device facts relevant to bench testing

The MX2203 is a Bluetooth HOBO TidbiT temperature logger with water-detect capability. It can be configured so Bluetooth advertising turns off while the logger detects water and turns back on after it is removed from water.

For discovery testing:

- keep the MX2203 dry;
- keep HOBOconnect disconnected from it;
- isolate/wrap nearby MX2201 and MX2001 loggers so the scanner does not connect to them first.

## What this diagnostic firmware does

1. Starts an active BLE scan after normal Meshtastic Bluetooth startup.
2. Looks for Onset/HOBO candidates using:
   - Onset manufacturer prefix `C5 00`;
   - the common HOBO 128-bit service already proven on MX2201/MX2001;
   - a local name containing `HOBO` or `MX2203`.
3. Logs the candidate MAC and RSSI.
4. Dumps the complete BLE advertisement payload in hex.
5. Dumps every advertisement field, including AD type and payload bytes.
6. Attempts to connect.
7. Tests whether the MX2203 exposes the same HOBO command service and characteristic used by MX2201/MX2001.
8. If the common command channel exists, enables notifications, sends the already-proven HOBO `INIT` command, then sends the common `NEWREAD64` live-read request.
9. Prints every returned notification as raw hex without assuming an MX2201/MX2001 decoder.

## Commands currently probed

INIT:

```text
01 01 04 05 1C 01 00
```

NEWREAD64:

```text
01 01 08 04 04 00 00 00 00 00 00
```

These commands are used only after the logger is shown to expose the same command service/characteristic already proven on MX2201/MX2001.

## What we need from the first serial capture

The useful lines begin with:

```text
MX2203 DISCOVERY: ONSET/HOBO CANDIDATE
Candidate MAC:
MX2203 DISCOVERY ADV RAW
MX2203 ADV field
```

Then look for either:

```text
MX2203 DISCOVERY: known HOBO service FOUND
```

or:

```text
MX2203 DISCOVERY: known MX2201/MX2001 HOBO service NOT found
```

If the common service exists, the most important next line is:

```text
MX2203 DISCOVERY RX NOTIFY
```

That raw response will determine whether MX2203 uses the same live-temperature packet layout or requires a new decoder/protocol path.

## Build target

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e seeed_xiao_nrf52840_kit
```

Upload:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e seeed_xiao_nrf52840_kit -t upload
```

Serial monitor:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor -b 115200
```

## Scope

This branch is discovery-only. It does not yet transmit MX2203 data over Meshtastic and does not alter the finalized MX2201/MX2001 implementation.
