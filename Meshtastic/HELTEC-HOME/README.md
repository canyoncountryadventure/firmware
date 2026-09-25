# Heltec V4 environmental gateway — Gateway v4

**Branch:** Heltec-Gateway-v4  
**Device:** Heltec V4, PlatformIO environment \`heltec-v4\`  
**Purpose:** Receive allowed field-sensor packets over Meshtastic LoRa and independently upload their measurements over Wi-Fi to \`https://meshtastic-ecru.vercel.app/api/ingest\`.

## Allowed node numbers

| Station label on Vercel | Meshtastic ID | Decimal ID | Telemetry |
|---|---|---:|---|
| Hidden Valley (replacement) | !4aab9211 | 1252758033 | Temperature |
| Pack Creek | !fccdcc93 | 4241345683 | Temperature and stage |
| Wingate Moisture | !77788479 | 2004386937 | Soil moisture |
| Cliff Sensor | !c9f9f6e7 | 3388602087 | Temperature |
| Moab Heltec | !a35a4bf4 | 2740603892 | Local BLE HOBO |
| Fishlake Hightop | !5e021e35 | 1577197109 | Temperature |
| It's a Swell Day | !742ecff5 | 1949224949 | Temperature |
| Thousand Lake Mountain | !9df66d7e | 2650172798 | Temperature |

The retired Hidden Valley !b57d051f/3044869407 is **not** allowed to upload. The Heltec does not need hard-coded station names: HTTPS requests include the numeric sender and canonical labels are assigned by Vercel. The Heltec may still retain historical mesh NodeDB names for display and normal Meshtastic operation.

## Formats

* \`TELEMETRY_APP\` with an environmental temperature and device/battery telemetry.
* \`PRIVATE_APP\` 19-byte \`MX\` versioned MX2001 temperature/stage.
* \`PRIVATE_APP\` 8-byte \`SM\` v1 soil moisture and ADC10 from Wingate Moisture only. The paired standard soil-moisture report is not also uploaded, avoiding duplicated samples.
* \`PRIVATE_APP\` 24-byte \`DS\` v1 ultrasonic raw distance/stage from Pack Creek only. Uncalibrated stage is not uploaded as a water-level value.
* The existing 16-byte \`RK\` legacy rock-test parser is retained for backward compatibility.

## Build and update

GitHub Actions: [Build Heltec Gateway v4](https://github.com/canyoncountryadventure/firmware/actions/workflows/build_cca_heltec_gateway.yml).

After a successful build, download the artifact associated with the **latest source commit**, or the published \`downloads/Heltec-Gateway-v4.zip\` from the repository's \`field-self-recovery\` branch once its build finishes. Several builds can run concurrently; confirm the source commit before flashing.

Only install the **normal non-factory Heltec V4 application firmware** via the existing Wi-Fi Unified OTA updater. This preserves Meshtastic channel keys, NodeDB, Wi-Fi settings, and the gateway configuration. Do not install a \`*.factory.bin\` as a routine OTA update, and do not erase NVS.

The Actions workflow injects the Vercel ingest key from the repository secret \`HOBO_HTTP_GATEWAY_INGEST_KEY\`; do not commit that key into source. Verify an actual field reading logs \`CCA clean gateway: cloud accepted packet ... HTTP=201\`, then confirm \`/api/readings\` contains that node ID and packet type before deploying additional stations.
