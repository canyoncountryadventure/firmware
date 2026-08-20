# Universal HOBO Reader: MX2001 + MX2201 + MX2203

This is the production implementation used by branch `hobo-mx2001-mx2201-mx2203`.

## Supported hardware

- Seeed XIAO nRF52840 + Wio-SX1262
- RAK4631 / RAK19003

Both targets were hardware-validated with MX2001, MX2201, and MX2203 on 2026-08-19.

## Supported logger data

- **MX2001:** live water level + temperature
- **MX2201:** live temperature
- **MX2203:** live temperature

## Automatic telemetry timing

Automatic telemetry is controlled by the HOBO logger's own `STATUS` response, not by a free-running radio timer.

After connecting, the bridge performs a probe `NEWREAD64` only to identify the model and verify that live data can be decoded. The probe measurement is **not** automatically transmitted. This prevents an off-cycle startup packet.

The bridge then requests `STATUS`, which supplies:

- the current logger write pointer
- the logger logging interval in seconds

On a fresh connection or reboot, knowing the interval is not enough to know where the logger currently is inside that interval. The bridge therefore polls the write pointer until it observes the first real pointer change. That first pointer transition establishes the actual logger record phase. A fresh `NEWREAD64` is then performed and exactly one automatic Meshtastic packet is queued.

After the phase is established, the bridge waits until shortly before the next expected logger record, then fine-polls the write pointer. A pointer change triggers exactly one fresh `NEWREAD64` and one automatic Meshtastic transmission.

Automatic transmissions are therefore **pointer-gated**:

1. `STATUS` confirms a new logger record.
2. The bridge records the new write pointer.
3. A fresh `NEWREAD64` is performed.
4. The measurement is queued to Meshtastic.
5. Only after a successful queue operation is the write pointer marked as transmitted.

If the mesh packet cannot be queued, the pointer is retained and the bridge retries rather than silently consuming that record.

A direct manual `READ` does not consume the automatic write pointer and does not reset the automatic schedule.

If repeated `STATUS` requests fail, automatic telemetry **pauses**. The bridge keeps retrying `STATUS` and resumes only after write-pointer tracking recovers. It does not fall back to blind periodic transmissions because those cannot guarantee alignment with the logger's actual record time.

The serial log includes explicit timing diagnostics such as:

```text
HOBO universal: new logger record model=MX2201 old=0x00001ADB new=0x00001ADD interval=20 sec phase=LOCKED
HOBO universal AUTO TX confirmed count=7 pointer=0x00001ADD interval=20 sec pointer_to_tx=412 ms cadence=20037 ms
```

`pointer_to_tx` is the time from detecting the new HOBO record to queuing its Meshtastic packet. `cadence` is the elapsed time since the preceding automatic packet. These diagnostics make interval behavior directly testable on hardware.

### Model-specific automatic output

- **MX2001:** sends the existing 19-byte `PRIVATE_APP` packet containing water level, temperature, logger MAC, sequence, and BLE RSSI. This remains compatible with `tools/mx2001_receiver.py` and the water dashboard ingest path.
- **MX2201:** sends standard Meshtastic environmental temperature telemetry on `TELEMETRY_APP`.
- **MX2203:** sends standard Meshtastic environmental temperature telemetry on `TELEMETRY_APP`.

HOBO automatic packets use reliable queue priority. Actual RF airtime can still be delayed by normal LoRa channel activity, but the packet is generated and queued immediately after the new logger record is confirmed and read.

## Persistent logger assignment

A radio can be permanently assigned to one physical HOBO by BLE MAC address. This prevents a field node from reconnecting to a different nearby logger after a reboot, temporary obstruction, or BLE disconnect.

The assignment is saved in the node's internal flash at:

```text
/prefs/hobo_lock.bin
```

The record contains a version marker, the raw six-byte BLE address, and a checksum. It is restored automatically when the HOBO bridge starts.

When a lock is active, advertisements from every other HOBO are ignored. If the assigned logger is unavailable, the radio waits for that exact logger instead of silently switching to another one.

## Meshtastic commands

Commands are direct messages to the radio. A leading slash is optional.

### `LOGGER`

Reports the current logger, interval, BLE signal, and persistent assignment state without taking a measurement.

Unlocked example:

```text
HOBO CONNECTED
Model: MX2203
MAC: E2:16:BF:17:50:2E
BLE: -63 dBm
Interval: 600 sec
Lock: OFF
```

Locked example:

```text
HOBO CONNECTED
Model: MX2203
MAC: E2:16:BF:17:50:2E
BLE: -63 dBm
Interval: 600 sec
Lock: ON
Target: E2:16:BF:17:50:2E
```

If the target is temporarily unavailable:

```text
HOBO NOT CONNECTED
Lock: ON
Target: E2:16:BF:17:50:2E
Waiting for target
```

### `LOCK`

Locks the radio to the currently connected, positively identified MX2001, MX2201, or MX2203 and saves the BLE address to internal flash.

Example reply:

```text
LOGGER LOCKED
Model: MX2203
MAC: E2:16:BF:17:50:2E
Persists after reboot
```

A lock is accepted only after the current HOBO has been identified as a supported model.

### `UNLOCK`

Deletes the persistent assignment, releases the current HOBO BLE connection, and returns the node to discovery mode.

Example reply:

```text
LOGGER UNLOCKED
Scanning any supported HOBO
Current BLE link will be released
```

### `READ`

Performs one fresh BLE `NEWREAD64` read and replies directly to the requester. The reply includes:

- HOBO model
- full BLE MAC address
- BLE RSSI captured when the radio selected the logger
- current measurement

Example:

```text
MX2001
Logger: F1:0D:9D:29:C3:2D
BLE: -63 dBm
Level: 0.91 ft
Temp: 75.4 F
```

A manual `READ` is independent of the automatic pointer-gated telemetry schedule.

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

STATUS:

```text
01 01 08 04 05 00 00 00 00 00 00
```

The status response supplies the logger write pointer and logging interval used by automatic telemetry scheduling.

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

## Discovery

The reader discovers Onset candidates dynamically from manufacturer data, the common HOBO service UUID, or a HOBO/MX local name. No production logger MAC is hard-coded into the firmware.

Without a persistent lock, discovery order determines which valid HOBO the radio selects. With a lock, only the saved BLE address is eligible for connection.

The first model probe is always:

1. `INIT`
2. direct `NEWREAD64`

This directly identifies MX2201 and MX2203 and normally identifies MX2001. For older MX2001/MX2201 behavior that requires metadata setup, the proven MX2001 and MX2201 metadata fallback commands are retained.

A positively identified MX2203 advertisement does not receive MX2001/MX2201 metadata fallback commands.

## Board integration

`HOBOMX2001MX2201MX2203Telemetry.cpp` is the shared implementation used by the Seeed build.

`HOBOMX2001MX2201MX2203TelemetryRAK.cpp` is the RAK4631 compile adapter that reuses that exact implementation after the shared dependencies are loaded under the real `RAK_4631` configuration.

The RAK Meshtastic module hook still uses the historical `MX2001Diagnostic` include name, but on the canonical universal branch that header is only an alias to `HOBOMX2001MX2201MX2203TelemetryModule`. The old MX2001-only RAK implementation is not compiled on this branch.

## BLE connection model

The nRF52 Bluetooth layer allocates:

- one BLE peripheral link for a Meshtastic phone connection
- one BLE central link for the HOBO logger

The HOBO reader intentionally maintains only one logger BLE connection at a time.

## Code provenance and validation

- MX2001 automatic write-pointer scheduling and `PRIVATE_APP` packet behavior come from the hardware-proven MX2001 production sender.
- MX2201 automatic telemetry behavior follows the earlier hardware-proven interval-aligned telemetry implementation.
- MX2001 + MX2201 live BLE behavior is based on the hardware-proven combined reader.
- MX2203 response parsing is based on the hardware-proven MX2203 reader.
- MX2203 temperature conversion comes from HOBOconnect `OnsetSDK.dll`, not a fitted field equation.
- RAK4631 universal BLE behavior has been physically validated with MX2001, MX2201, and MX2203.
- MX2001, MX2201, and MX2203 `STATUS` responses have now all been observed on hardware, including logging intervals and changing write pointers.

### Final RAK4631 timing validation — 2026-08-19

The final pointer-gated firmware was bench-tested on physical RAK4631 hardware against MX2001, MX2201, and MX2203 loggers.

For MX2201 logger `E4:27:8C:B9:F4:B8` configured for a 20-second logging interval, consecutive automatic packets were observed at:

```text
count=1  pointer_to_tx=202 ms  cadence=0 ms
count=2  pointer_to_tx=202 ms  cadence=19848 ms
count=3  pointer_to_tx=202 ms  cadence=19879 ms
```

The measured non-baseline cadence averaged 19.864 seconds. Each automatic packet was generated only after a `STATUS` write-pointer change and a fresh `NEWREAD64` read. No free-running automatic timer is used for record generation.

Persistent logger assignment was also validated across a real RAK4631 reboot: the saved BLE MAC was restored from `/prefs/hobo_lock.bin`, non-target HOBO advertisements were ignored, and the node reconnected only to the saved logger.

Field deployment workflow is intentionally simple:

1. Flash the universal firmware.
2. Place the radio beside the intended HOBO.
3. Send `LOGGER` and verify model, MAC, and logging interval.
4. Send `LOCK` only when physically installing that radio with that logger.
5. Reboot once and use `LOGGER` to verify the saved target before leaving the site.
