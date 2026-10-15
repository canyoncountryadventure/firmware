# Diagnostic 8 upgrade and rollback

The final V5 package preserves the October 15 implementation reported to read the MX2001 successfully. See the [current README](../../README.md) for the complete matched release and the [final audit](V5-FINAL-AUDIT.md) for remaining hardware checks. No station was flashed, reset or erased by this audit.

## Before updating

1. Save the node's current firmware/version and a known-good app-only rollback UF2/DFU ZIP. The immutable V3 source reference is in the audit; a source branch alone is not a ready rollback binary.
2. Record node identity, channels and encryption configuration privately, `LOGGER`, target lock, `WATER STATUS` and `WATER CAL STATUS`. Preserve existing station configuration. Do not issue factory reset, erase, `WATER RESET`, `WATER CAL RESET`, or a sensor-change command during this upgrade.
3. Select SEN0313 or MB7388 based on actual wiring. Existing valid saved sensor selection wins over the build's default; changing firmware flavor does not convert wiring or calibration.
4. Download the matching app-only UF2 or OTA ZIP and `SHA256SUMS.txt` from the V5 branch's download folder. Check with `sha256sum -c SHA256SUMS.txt` (download all listed files, or verify the selected line). Never substitute factory/merged/full-flash images.

## USB UF2

Connect USB to the RAK4631 and enter its existing UF2 bootloader using the supported double-reset procedure. Copy the matching `.uf2` to its bootloader drive. This app-only image leaves bootloader, SoftDevice and filesystem addresses outside its payload. Wait for application startup, then check `VERSION`, `LOGGER`, mesh communication, `WATER STATUS` and calibration against the recorded values. Do not format the drive or use full-chip erase. Physical retention still requires this check.

## Existing drone/Scout DFU

The existing autonomous Scout embeds one exact target application. Flash the matching **`Drone-RAK-HOBO-v5-Audit-Repair-SEN0313.uf2` or `...-MB7388.uf2` onto the separate drone RAK4631**. Never flash a `Drone-*.uf2` onto the field station. Its manifest records both the exact target commit/OTA checksum and unchanged Scout implementation commit `37306158a64266fd05dfcd3bf01c5b47d0dc35fa`. The compressed image and DAT must match the chosen field target.

Power the matched Scout and send the existing `DFU` mesh DM to the intended field node. The target acknowledges and enters AdaDFU after approximately three seconds; the autonomous Scout discovers the eligible AdaDFU bootloader and transfers its embedded image. Enter DFU on only the intended target during this operation. Wait for completion and verify the post-update mesh callback/version and configuration. The source still uses the proven legacy service `0x1530`; no flasher protocol or recovery logic was changed.

The matching **application-only Nordic/Adafruit OTA ZIP** is also provided. It is accepted by the existing Phase-2 serial Scout uploader `tools/rak_dfu_serial_upload.py` on the pinned drone branch. A ZIP alone does not replace the embedded image inside an already-flashed autonomous Scout. Both the standard ZIP and the matched autonomous Scout UF2 are provided; physical transfer remains unverified.

Bootloader entry and transfer deliberately restart the station. Perform them only as part of the actual field upgrade. No bootloader or SoftDevice update is included.

## Rollback

If discovery, model, telemetry, water reading or configuration checks fail, re-enter the existing bootloader and install the saved known-good **app-only** rollback image using the same USB or Scout procedure. Do not erase flash. Recheck identity/channels/keys, HOBO lock and water calibration. Prefer the station's exact previous build or a verified compatible V3 water build. V3 lacks some newer MaxBotix behavior; do not assume it supports the same physical sensor. No fabricated or unverified V3 rollback binary is provided here.

On unexpected missing configuration, stop configuration writes and preserve the device for diagnosis. The repair does not include a filesystem snapshot/restore mechanism, and cannot restore calibration that an older firmware already erased.
