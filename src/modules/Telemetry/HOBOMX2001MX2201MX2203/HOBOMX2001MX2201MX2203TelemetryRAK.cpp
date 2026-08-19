#include "configuration.h"

#if defined(ARCH_NRF52) && defined(RAK_4631)

// Load every shared dependency under the real RAK4631 board configuration
// before reusing the hardware-proven universal implementation below.
#include "HOBOMX2001MX2201MX2203Telemetry.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "main.h"
#include <bluefruit.h>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

// The universal implementation is currently compile-gated by the Seeed target.
// For this isolated RAK4631 validation branch only, expose that implementation
// after all hardware/configuration headers have already been processed as RAK.
// This keeps the proven protocol implementation identical while avoiding any
// Seeed-specific board configuration leaking into the RAK build.
#define SEEED_XIAO_NRF52840_KIT 1
#include "HOBOMX2001MX2201MX2203Telemetry.cpp"
#undef SEEED_XIAO_NRF52840_KIT

#endif
