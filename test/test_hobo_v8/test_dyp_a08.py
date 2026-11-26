"""Exercise the actual V8 DYP driver with a fake UART and clock via native C++."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / "src/modules/Telemetry/DistanceSensor/DistanceSensorDrivers.cpp").read_text()
module = (root / "src/modules/Telemetry/DistanceSensor/DistanceSensorModule.cpp").read_text()
head = (root / "src/modules/Telemetry/DistanceSensor/DistanceSensorModule.h").read_text()
variant = (root / "variants/nrf52840/rak4631/platformio.ini").read_text()
firmware_label = (root / "src/modules/Telemetry/HOBOSelfRecovery/HOBOSelfRecovery.cpp").read_text()

assert "[env:rak4631_water_unified_v8]" in variant
assert 'FIRMWARE_LABEL[] = "RAK WATER DISTANCE + HOBO V8"' in firmware_label
assert "DYP_A08_CONTROLLED" in head and "DYPA08ControlledDistanceDriver a08Driver;" in head
assert "requested = DistanceSensorType::DYP_A08_CONTROLLED;" in module
assert "WATER SENSOR DYP-A08" in module
assert "if (!alwaysPoweredControlled)" in module
assert "setDistanceSensorPower(true);" in module
assert "activeType == DistanceSensorType::DYP_A08_CONTROLLED ? 200UL : 80UL" in module
assert "nextSampleMs = alwaysPoweredControlled ? samplePowerOnMs : samplePowerOnMs + SENSOR_POWER_SETTLE_MS;" in module

def function(signature):
    start = source.index(signature)
    opening = source.index("{", start)
    end, depth = opening + 1, 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]

methods = "\n\n".join([function("bool DYPA08ControlledDistanceDriver::begin()"),
                         function("DistanceReading DYPA08ControlledDistanceDriver::read()")])

program = r"""
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
uint32_t clockMs = 10000;
uint32_t millis() { return clockMs; }
void delay(unsigned m) { clockMs += m; }
enum class DistanceReadStatus { OK, NOT_READY, TIMEOUT, BAD_FRAME, CHECKSUM_ERROR, OUT_OF_RANGE, IO_ERROR };
struct DistanceReading {
    DistanceReadStatus status;
    uint32_t distanceMm;
    uint32_t measuredAtMs;
    bool valid() const {return status==DistanceReadStatus::OK;}
};
DistanceReading makeReading(DistanceReadStatus s, uint32_t d=0) {return {s,d,millis()};}
constexpr uint32_t UART_FRAME_TIMEOUT_MS = 450;
struct FakeSerial {
    std::vector<uint8_t> queued, prepared;
    unsigned triggers = 0;
    uint8_t lastByte = 0xff;
    void begin(int baud) { assert(baud==9600); }
    int available() { return (int)queued.size(); }
    int read() { auto x=queued[0]; queued.erase(queued.begin()); return x; }
    size_t write(uint8_t b) {
        assert(b==0); ++triggers; lastByte=b; queued=prepared; return 1;
    }
    void flush() {}
} Serial1;
struct DYPA08ControlledDistanceDriver {
    bool initialized = false;
    uint32_t lastTriggerMs = 0;
    bool begin();
    DistanceReading read();
};
""" + methods + r"""
int main() {
    DYPA08ControlledDistanceDriver d;
    assert(d.begin());
    Serial1.prepared = {0xFF,0x07,0xA1,0xA7};
    auto r = d.read();
    assert(r.valid() && r.distanceMm==1953 && Serial1.triggers==1);
    assert(d.read().status==DistanceReadStatus::NOT_READY && Serial1.triggers==1);
    clockMs+=200;
    Serial1.prepared={0xFF,0x07,0xA1,0x00};  // bad checksum
    assert(d.read().status==DistanceReadStatus::CHECKSUM_ERROR);
    clockMs+=200;
    Serial1.prepared={0xFF,0x00,0x00,0xFF};  // no return
    assert(d.read().status==DistanceReadStatus::OUT_OF_RANGE);
    clockMs+=200;
    Serial1.prepared={};  // no serial frame
    assert(d.read().status==DistanceReadStatus::TIMEOUT);
    clockMs+=200;
    Serial1.prepared={0x13,0x00,0xFF,0x07,0xA1,0xA7}; // resync
    assert(d.read().valid());
    assert(Serial1.triggers==5);
    puts("PASS: live C++ DYP UART controlled protocol, 1953 mm, throttling, checksum, timeout and resync");
}
"""

with tempfile.TemporaryDirectory(prefix="dyp-a08-") as directory:
    path = Path(directory) / "dyp_a08.cpp"
    executable = Path(directory) / "dyp_a08"
    path.write_text(program)
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", str(path), "-o", str(executable)],check=True)
    subprocess.run([str(executable)], check=True)
