# Second audit — October 14, 2026

Reviewed the published first repair at `40066e76b5907d4e9fd81f46eb7e4c5078ddb1b1`, its field firmware source `9008cc210e0079d341cce65a453d1affcd920cbf`, current V5 head `b24de43d86ba1238fb29edd3006ec63174701137`, lifecycle functions, manual locks, recovery flags, protocol/storage checks and field/Scout packaging. GitHub run `38636048833` completed successfully, including both matched Scouts and publication; those files existed before this second review.

## Additional confirmed defect

| Path | Before | Correction | Evidence/confidence |
| --- | --- | --- | --- |
| Failed scan stop plus rejected central connection request | `Scanner.stop()` can fail without clearing the SDK running flag. `Central.connect()` then fails. The repair cleared `connecting`, backed off, but did not request scan restart. `runOnce()` sees cached `isRunning()==true` and performs no scanner operation, leaving a paused/stopped scan stalled indefinitely | Set `scanRestartRequested=true` on failed connection submission, using the existing resume-first path and five-second backoff | Real lifecycle functions compiled against failure-injected boundaries; new regression fails before the patch (`Scanner.starts==1`) and passes afterward. Defect is conditional but deterministic. Its occurrence at a particular station remains unproven |
| Canceled attempt with stale scanner running flag | A failed stop followed by a pending connection that is canceled can leave the same cache state | Explicitly request scan recovery after unconnected recovery/cancellation; pending/live central links still prevent start | Second failure-injection case exercises actual main-loop dispatch. No periodic kick, increased timeout or hardware reset added |
| V5 front page offered conflicting releases | Repair links were added above old V5 downloads, old Scout target hashes and obsolete 50% scan/45-second setup/watchdog descriptions | Replace with one release table generated from the newly verified artifacts; build both matched Scouts for each source commit | Original source strings and existing file paths directly verified. Publication checks both field images/DFU ZIPs and Scout manifests before writing links |

Pinned SDK evidence: [`BLEScanner.cpp`, lines 115–145](https://github.com/meshtastic/Adafruit_nRF52_Arduino/blob/0fd295f13203e93df19d578073646ec32f2bf45a/libraries/Bluefruit52Lib/src/BLEScanner.cpp#L115-L145). `stop()` returns early on SDK error; `_runnning=false` is reached only on success. `isRunning()` exposes that cached flag. Repair source before correction: [`scanCallback()`, lines 744–752](https://github.com/canyoncountryadventure/firmware/blob/9008cc210e0079d341cce65a453d1affcd920cbf/src/modules/Telemetry/HOBOMX2001MX2201MX2203/HOBOMX2001MX2201MX2203Telemetry.cpp#L744-L752), [`triggerBleRecovery()`](https://github.com/canyoncountryadventure/firmware/blob/9008cc210e0079d341cce65a453d1affcd920cbf/src/modules/Telemetry/HOBOMX2001MX2201MX2203/HOBOMX2001MX2201MX2203Telemetry.cpp#L908-L933), and the `runOnce()` scanner decision at lines 1342–1355.

## Rechecked conclusions

The deferred callback discovery path remains compatible with the pinned independent BLE event task. The passive raw event observer does not suppress library Discovery/GATT handling. Candidate filtering forwards address type; non-connectable packets do not establish a connectable link. UUIDs, model decoding, lock/identity storage, water calibration schema, sensor drivers and the remote DFU implementation remain unchanged under the preservation checks.

The newly reproduced scanner stall is additional evidence, not a claim that V4's onset or either supplied physical error has now been conclusively reproduced. Main/callback protocol state still follows the inherited V3 design; these host boundary tests do not certify every real RTOS interleaving. No broader synchronization rewrite was introduced without hardware timing validation.

The user now explicitly requested editing `RAK-Water-Distance-HOBO-v5`. The update keeps V3/V4 and the original drone implementation unchanged. The dedicated first-repair branch remains available as an audit/rollback checkpoint.
