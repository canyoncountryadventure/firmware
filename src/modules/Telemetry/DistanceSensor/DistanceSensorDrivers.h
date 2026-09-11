#pragma once

#include "DistanceSensorDriver.h"

#if defined(DISTANCE_SENSOR_NODE) && defined(ARCH_NRF52)

class SEN0590DistanceDriver : public DistanceSensorDriver
{
  public:
    bool begin() override;
    DistanceReading read() override;
    const char *name() const override { return "SEN0590"; }
    const char *interfaceName() const override { return "I2C"; }
    DistanceSensorType type() const override { return DistanceSensorType::SEN0590; }
    bool probe();

  private:
    bool initialized = false;
};

class DFRobotUARTDistanceDriver : public DistanceSensorDriver
{
  public:
    explicit DFRobotUARTDistanceDriver(DistanceSensorType configuredType = DistanceSensorType::UART_GENERIC);
    bool begin() override;
    DistanceReading read() override;
    const char *name() const override;
    const char *interfaceName() const override { return "UART 9600"; }
    DistanceSensorType type() const override { return configuredType; }
    void setType(DistanceSensorType type);

  private:
    DistanceSensorType configuredType;
    bool initialized = false;
    uint32_t minimumMm() const;
    uint32_t maximumMm() const;
};

#endif
