#pragma once
#include "configuration.h"
#if defined(ARCH_ESP32) && HAS_WIFI && defined(HELTEC_V4)
#include <Arduino.h>

// V10 self-hosted OTA stage: keep the existing Meshtastic OTA_1 loader at the
// beginning of its partition. Store the signed/hashed app image in unused
// OTA_1 space AFTER a reserved 640 KiB loader area. The companion loader,
// installed independently by USB, copies it into OTA_0 on its next boot.
namespace CcaRemoteOtaStage {
  bool stageAndReboot(const String &version, const String &url, const String &sha256, String &reason);
  String statusJson();
  String healthNote();
}
#endif
