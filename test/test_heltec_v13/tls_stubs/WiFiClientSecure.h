#pragma once
#include "esp_heap_caps.h"
#include <cerrno>
#include <cstdint>

extern bool testConnectionFails;
extern unsigned testConnections;
extern unsigned testDestructions;

class WiFiClientSecure
{
  public:
    virtual ~WiFiClientSecure()
    {
        if (allocated)
            testFree += 40000;
        ++testDestructions;
    }
    virtual int connect(const char *, uint16_t, int32_t)
    {
        ++testConnections;
        testFree -= 40000;
        testMinimum = testFree;
        if (testConnectionFails) {
            testFree += 40000;
            errno = ENOMEM;
            return 0;
        }
        allocated = true;
        return 1;
    }
    int lastError(char *, size_t) { return testConnectionFails ? -16 : 0; }

  private:
    bool allocated = false;
};
