#include "configuration.h"

#if defined(DISTANCE_SENSOR_NODE) && defined(ARCH_NRF52)

#include "DistanceSensorDrivers.h"
#include "Throttle.h"

#include <Wire.h>

namespace
{
static constexpr uint8_t SEN0590_ADDRESS = 0x74;
static constexpr uint8_t SEN0590_COMMAND_REGISTER = 0x10;
static constexpr uint8_t SEN0590_MEASURE_COMMAND = 0xB0;
static constexpr uint8_t SEN0590_DISTANCE_REGISTER = 0x02;
static constexpr uint32_t UART_FRAME_TIMEOUT_MS = 180;

DistanceReading makeReading(DistanceReadStatus status, uint32_t distanceMm = 0)
{
    DistanceReading r;
    r.status = status;
    r.distanceMm = distanceMm;
    r.measuredAtMs = millis();
    return r;
}
} // namespace

const char *distanceSensorTypeName(DistanceSensorType type)
{
    switch (type) {
    case DistanceSensorType::AUTO:
        return "AUTO";
    case DistanceSensorType::SEN0590:
        return "SEN0590";
    case DistanceSensorType::SEN0311_A02YYUW:
        return "SEN0311/A02YYUW";
    case DistanceSensorType::SEN0313_A01NYUB:
        return "SEN0313/A01NYUB";
    case DistanceSensorType::UART_GENERIC:
        return "UART-ULTRASONIC";
    default:
        return "NONE";
    }
}

const char *distanceReadStatusName(DistanceReadStatus status)
{
    switch (status) {
    case DistanceReadStatus::OK:
        return "OK";
    case DistanceReadStatus::NOT_READY:
        return "NOT_READY";
    case DistanceReadStatus::TIMEOUT:
        return "TIMEOUT";
    case DistanceReadStatus::BAD_FRAME:
        return "BAD_FRAME";
    case DistanceReadStatus::CHECKSUM_ERROR:
        return "CHECKSUM";
    case DistanceReadStatus::OUT_OF_RANGE:
        return "OUT_OF_RANGE";
    case DistanceReadStatus::IO_ERROR:
        return "IO_ERROR";
    default:
        return "NO_SENSOR";
    }
}

bool SEN0590DistanceDriver::probe()
{
    Wire.begin();
    Wire.beginTransmission(SEN0590_ADDRESS);
    return Wire.endTransmission() == 0;
}

bool SEN0590DistanceDriver::begin()
{
    Wire.begin();
    initialized = probe();
    return initialized;
}

DistanceReading SEN0590DistanceDriver::read()
{
    if (!initialized && !begin())
        return makeReading(DistanceReadStatus::NO_SENSOR);

    // DFRobot SEN0590 protocol: write 0xB0 to register 0x10, wait for the
    // measurement, then read the 16-bit big-endian distance from register 0x02.
    Wire.beginTransmission(SEN0590_ADDRESS);
    Wire.write(SEN0590_COMMAND_REGISTER);
    Wire.write(SEN0590_MEASURE_COMMAND);
    if (Wire.endTransmission() != 0)
        return makeReading(DistanceReadStatus::IO_ERROR);

    delay(50);

    Wire.beginTransmission(SEN0590_ADDRESS);
    Wire.write(SEN0590_DISTANCE_REGISTER);
    if (Wire.endTransmission(false) != 0)
        return makeReading(DistanceReadStatus::IO_ERROR);

    const uint8_t requested = 2;
    const uint8_t received = Wire.requestFrom(SEN0590_ADDRESS, requested);
    if (received != requested || Wire.available() < 2)
        return makeReading(DistanceReadStatus::TIMEOUT);

    const uint16_t raw = (static_cast<uint16_t>(Wire.read()) << 8) |
                         static_cast<uint16_t>(Wire.read());

    // DFRobot's reference implementation applies a +10 mm compensation.
    const uint32_t mm = static_cast<uint32_t>(raw) + 10U;
    if (mm < 20U || mm > 4000U)
        return makeReading(DistanceReadStatus::OUT_OF_RANGE, mm);

    return makeReading(DistanceReadStatus::OK, mm);
}

DFRobotUARTDistanceDriver::DFRobotUARTDistanceDriver(DistanceSensorType configuredType) : configuredType(configuredType) {}

void DFRobotUARTDistanceDriver::setType(DistanceSensorType type)
{
    configuredType = type;
    initialized = false;
}

bool DFRobotUARTDistanceDriver::begin()
{
    Serial1.begin(9600);
    initialized = true;
    return true;
}

const char *DFRobotUARTDistanceDriver::name() const
{
    return distanceSensorTypeName(configuredType);
}

uint32_t DFRobotUARTDistanceDriver::minimumMm() const
{
    switch (configuredType) {
    case DistanceSensorType::SEN0311_A02YYUW:
        return 30U;
    case DistanceSensorType::SEN0313_A01NYUB:
        return 280U;
    default:
        return 20U;
    }
}

uint32_t DFRobotUARTDistanceDriver::maximumMm() const
{
    switch (configuredType) {
    case DistanceSensorType::SEN0311_A02YYUW:
        return 4500U;
    case DistanceSensorType::SEN0313_A01NYUB:
        return 7500U;
    default:
        return 8000U;
    }
}

DistanceReading DFRobotUARTDistanceDriver::read()
{
    if (!initialized)
        begin();

    // The A02YYUW/A01NYUB family emits 4-byte frames continuously:
    // FF, distance_hi, distance_lo, checksum.  Resynchronize on 0xFF and
    // validate checksum before accepting the reading.
    const uint32_t started = millis();
    uint8_t frame[4] = {};
    uint8_t index = 0;
    bool sawHeader = false;
    bool sawChecksumError = false;

    while (Throttle::isWithinTimespanMs(started, UART_FRAME_TIMEOUT_MS)) {
        while (Serial1.available() > 0 && Throttle::isWithinTimespanMs(started, UART_FRAME_TIMEOUT_MS)) {
            const uint8_t b = static_cast<uint8_t>(Serial1.read());

            if (index == 0) {
                if (b != 0xFF)
                    continue;
                frame[0] = b;
                index = 1;
                sawHeader = true;
                continue;
            }

            frame[index++] = b;
            if (index < sizeof(frame))
                continue;

            const uint8_t checksum = static_cast<uint8_t>(frame[0] + frame[1] + frame[2]);
            if (checksum != frame[3]) {
                sawChecksumError = true;
                index = 0;
                continue;
            }

            const uint32_t mm = (static_cast<uint32_t>(frame[1]) << 8) |
                                static_cast<uint32_t>(frame[2]);
            if (mm < minimumMm() || mm > maximumMm())
                return makeReading(DistanceReadStatus::OUT_OF_RANGE, mm);

            return makeReading(DistanceReadStatus::OK, mm);
        }
        delay(1);
    }

    if (sawChecksumError)
        return makeReading(DistanceReadStatus::CHECKSUM_ERROR);
    if (sawHeader)
        return makeReading(DistanceReadStatus::BAD_FRAME);
    return makeReading(DistanceReadStatus::TIMEOUT);
}

#endif
