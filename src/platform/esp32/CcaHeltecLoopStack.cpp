// Heltec V4 / CCA gateway stack fix, 2026-11-25.
//
// The gateway's Cloudflare TLS handshake runs in Arduino loopTask via
// CcaCloudRemote::runOnce(). On the 8 KiB Arduino default, ESP32-S3 reset
// reproducibly at 23.6 seconds with:
//   "Stack canary watchpoint triggered (loopTask)"
// immediately after HTTPClient::sendRequest GET /v10/config.
//
// Arduino-ESP32 main.cpp defines getArduinoLoopTaskStackSize() as a weak
// C++ symbol. Provide a strong per-target override instead of patching
// PlatformIO's toolchain, disabling stack protection or introducing a
// second runtime updater.
//
// 32 KiB leaves ample internal heap on the Heltec V4 (the failing build
// reported ~164 KiB free before the TLS request). No repartition or erase.
#include <stddef.h>
#include "configuration.h"
#if defined(ARCH_ESP32) && defined(HELTEC_V4) && HAS_WIFI && HOBO_HTTP_GATEWAY_ENABLED
size_t getArduinoLoopTaskStackSize(void)
{
    return 32U * 1024U;
}
#endif
