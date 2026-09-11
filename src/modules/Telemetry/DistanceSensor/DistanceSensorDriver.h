#pragma once

#include "configuration.h"

#if defined(DISTANCE_SENSOR_NODE) && defined(ARCH_NRF52)

#include <Arduino.h>
#include <cstdint>

// The application layer talks only to this interface.  Water/trail logic does
// not need to know whether the physical sensor is I2C or UART.
enum class DistanceSensorType : uint8_t
{
    AUTO = 0,
    SEN0590 = 1,
    SEN0311_A02YYUW = 2,
    SEN0313_A01NYUB = 3,
    UART_GENERIC = 4,
    NONE = 255
};

enum class DistanceReadStatus : uint8_t
{
    OK = 0,
    NOT_READY,
    TIMEOUT,
    BAD_FRAME,
    CHECKSUM_ERROR,
    OUT_OF_RANGE,
    IO_ERROR,
    NO_SENSOR
};

struct DistanceReading
{
    DistanceReadStatus status = DistanceReadStatus::NO_SENSOR;
    uint32_t distanceMm = 0;
    uint32_t measuredAtMs = 0;

    bool valid() const { return status == DistanceReadStatus::OK; }
};

class DistanceSensorDriver
{
  public:
    virtual ~DistanceSensorDriver() = default;
    virtual bool begin() = 0;
    virtual DistanceReading read() = 0;
    virtual const char *name() const = 0;
    virtual const char *interfaceName() const = 0;
    virtual DistanceSensorType type() const = 0;
};

const char *distanceSensorTypeName(DistanceSensorType type);
const char *distanceReadStatusName(DistanceReadStatus status);

#endif
