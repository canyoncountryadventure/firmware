#pragma once
#include <cstdint>
#include <string>

class String : public std::string
{
  public:
    using std::string::string;
    using std::string::operator=;
    String(const std::string &value) : std::string(value) {}
    String(int32_t value) : std::string(std::to_string(value)) {}
    String(uint32_t value) : std::string(std::to_string(value)) {}
};

extern uint32_t testNow;
inline uint32_t millis()
{
    return testNow;
}
