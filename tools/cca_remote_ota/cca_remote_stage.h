#pragma once
// Companion to the Heltec V10 CcaRemoteOtaStage module.
// Called only by the CCA-patched Meshtastic unified OTA_1 loader.
// Returns on "no pending OTA" or on recovery fallback; restarts upon success.
void cca_remote_stage_install();
