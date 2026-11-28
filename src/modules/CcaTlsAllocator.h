#pragma once
#include <cstdint>
namespace CcaTlsAllocator {
void install();
bool active();
uint32_t externalAllocations();
uint32_t internalFallbacks();
uint32_t allocationFailures();
}
