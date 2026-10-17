"""Execute production water-job methods with a fake clock, rail and sensor.

This checks scheduler return and power ownership, not physical BLE behavior.
"""

from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (
    root / "src/modules/Telemetry/DistanceSensor/DistanceSensorModule.cpp"
).read_text()
header = (
    root / "src/modules/Telemetry/DistanceSensor/DistanceSensorModule.h"
).read_text()


def function(signature):
    start = source.index(signature)
    opening = source.index("{", start)
    end, depth = opening + 1, 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


state = header[
    header.index("    enum class SampleJob") : header.index(
        "    bool reinitializeSensorPending"
    )
]
methods = "\n".join(
    function(s)
    for s in (
        "bool DistanceSensorModule::scheduleSample(",
        "bool DistanceSensorModule::sampleBusy()",
        "void DistanceSensorModule::cancelSample()",
        "void DistanceSensorModule::serviceSample(",
    )
)
program = (
    r"""
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
#define RAK_4631 1
#define LOG_INFO(...) ((void)0)
#define LOG_WARN(...) ((void)0)
uint32_t clockMs = 1000;
uint32_t millis() { return clockMs; }
constexpr uint32_t SENSOR_POWER_SETTLE_MS = 8000;
bool rail = false;
unsigned powerOns = 0, powerOffs = 0;
void setDistanceSensorPower(bool on) {
    rail = on;
    if (on) ++powerOns; else ++powerOffs;
}
namespace concurrency {
struct Lock {};
struct LockGuard { explicit LockGuard(Lock *) {} };
}
enum class DistanceSensorType { NONE, MB7388, UART_GENERIC };
enum class DistanceReadStatus { OK, TIMEOUT };
struct DistanceReading {
    DistanceReadStatus status = DistanceReadStatus::OK;
    uint32_t distanceMm = 1234;
    uint32_t measuredAtMs = 0;
    bool valid() const { return status==DistanceReadStatus::OK; }
};
struct FakeSerial { unsigned bytes=0; unsigned available(){return bytes;} void read(){--bytes;} } Serial1;
struct Driver {
    unsigned reads=0;
    bool ok=true;
    std::vector<uint32_t> values;
    std::vector<bool> validity;
    void begin() {}
    void setType(DistanceSensorType) {}
    DistanceReading read() {
        assert(rail);
        const auto value = values.empty() ? 1234 : values[reads % values.size()];
        const bool valid=ok && (validity.empty() || validity[reads % validity.size()]);
        ++reads;
        return {valid?DistanceReadStatus::OK:DistanceReadStatus::TIMEOUT, value, clockMs};
    }
};
struct DistanceSensorModule {
    Driver mb7388Driver, uartDriver;
    Driver *activeDriver = &mb7388Driver;
    DistanceSensorType activeType = DistanceSensorType::MB7388;
    DistanceReading latestReading;
    static constexpr uint8_t FLAG_CALIBRATED=1;
    struct { uint8_t flags=FLAG_CALIBRATED; uint8_t reserved0=0; int32_t stageReferenceMm=2000; } cfg;
    bool latestStageValid=false;
    int32_t latestStageMm=0;
"""
    + state
    + r"""
    unsigned telemetry=0, replies=0, verifies=0, calibrations=0;
    uint32_t median=0, low=0, high=0, replyTo=0;
    uint8_t replyChannel=0;
    int32_t calibratedStage=0;
    bool scheduleSample(SampleJob, uint32_t=0, uint8_t=0, int32_t=0);
    bool sampleBusy();
    void cancelSample();
    void serviceSample(uint32_t);
    DistanceReading readDistance(){latestReading=activeDriver->read();return latestReading;}
    bool sendDistanceTelemetry(){++telemetry;return true;}
    bool sendTextReply(uint32_t to,uint8_t ch,const char *){++replies;replyTo=to;replyChannel=ch;return true;}
    void replyStatus(uint32_t to,uint8_t ch,bool){sendTextReply(to,ch,"");}
    void replyRead(uint32_t to,uint8_t ch,bool){sendTextReply(to,ch,"");}
    void replyVerify(uint32_t to,uint8_t ch,uint32_t mid,uint32_t lo,uint32_t hi){
        ++verifies;median=mid;low=lo;high=hi;sendTextReply(to,ch,"");
    }
    bool finishCalibration(int32_t stage,uint32_t to,uint8_t ch,uint32_t mid,uint32_t lo,uint32_t hi){
        ++calibrations;calibratedStage=stage;median=mid;low=lo;high=hi;sendTextReply(to,ch,"");return true;
    }
};
"""
    + methods
    + r"""
void reset(uint32_t now=1000){clockMs=now;rail=false;powerOns=powerOffs=0;Serial1.bytes=0;}
int main() {
    using Job=DistanceSensorModule::SampleJob;
    // All result-producing jobs use the same full burst and one warm-up.
    for(auto job:{Job::SCHEDULED,Job::STATUS,Job::READ,Job::RAW,Job::TELEMETRY_NOW,Job::VERIFY,Job::CALIBRATE}) {
        reset();DistanceSensorModule m;
        m.mb7388Driver.values={100,10000,999,1001,1005,1003,1002,1004,1006,1007};
        assert(m.scheduleSample(job,42,3,-250));
        assert(clockMs==1000 && rail && powerOns==1 && m.sampleBusy());
        assert(!m.scheduleSample(Job::READ,99,1));
        unsigned otherWork=0;
        for(clockMs=1100;clockMs<9000;clockMs+=100) {
            const auto entered=clockMs;
            Serial1.bytes=6;m.serviceSample(clockMs);++otherWork;
            assert(clockMs==entered && rail && m.mb7388Driver.reads==0 && Serial1.bytes==0);
        }
        assert(otherWork==79);
        for(unsigned i=0;i<10;++i) {
            m.serviceSample(clockMs);
            assert(m.mb7388Driver.reads==i+1);
            if(i<9) {
                assert(rail && m.sampleBusy() && m.telemetry==0 && m.replies==0);
                clockMs+=79;m.serviceSample(clockMs);assert(m.mb7388Driver.reads==i+1);++clockMs;
            }
        }
        assert(!rail && !m.sampleBusy() && powerOffs==1 && m.mb7388Driver.reads==10);
        assert(m.latestReading.distanceMm==1004 && m.latestSampleCount==10 && m.latestSampleSpreadMm==9900);
        assert(m.latestStageValid && m.latestStageMm==996);
        assert(m.telemetry==(job==Job::SCHEDULED || job==Job::TELEMETRY_NOW));
        if(job!=Job::SCHEDULED) assert(m.replyTo==42 && m.replyChannel==3);
        if(job==Job::CALIBRATE)assert(m.calibrations==1 && m.calibratedStage==-250 && m.median==1004);
        if(job==Job::VERIFY)assert(m.verifies==1 && m.median==1004 && m.low==100 && m.high==10000);
    }
    // Invalid frames do not count, and ten valid frames can finish at attempt twenty.
    reset();DistanceSensorModule mixed;mixed.mb7388Driver.validity={false,true};
    assert(mixed.scheduleSample(Job::READ));clockMs=9000;
    for(unsigned i=0;i<20;++i){mixed.serviceSample(clockMs);clockMs+=80;}
    assert(!rail && mixed.latestSampleCount==10 && mixed.latestReading.valid() && mixed.mb7388Driver.reads==20);
    // Fewer than ten is a failure even when the last frame was valid.
    for(auto job:{Job::SCHEDULED,Job::READ,Job::VERIFY,Job::CALIBRATE}) {
        reset();DistanceSensorModule partial;
        partial.mb7388Driver.validity=std::vector<bool>(20,false);
        for(unsigned i=0;i<8;++i)partial.mb7388Driver.validity[i]=true;
        partial.mb7388Driver.validity[19]=true;
        assert(partial.scheduleSample(job));clockMs=9000;
        for(unsigned i=0;i<20;++i){partial.serviceSample(clockMs);clockMs+=80;}
        assert(!rail && !partial.sampleBusy() && !partial.latestReading.valid() && !partial.latestStageValid);
        assert(partial.latestSampleCount==9 && partial.calibrations==0 && partial.verifies==0);
    }
    reset();DistanceSensorModule invalid;invalid.mb7388Driver.ok=false;
    assert(invalid.scheduleSample(Job::CALIBRATE));clockMs=9000;
    for(unsigned i=0;i<20;++i){invalid.serviceSample(clockMs);clockMs+=80;}
    assert(!rail && !invalid.sampleBusy() && invalid.mb7388Driver.reads==20 && invalid.calibrations==0);
    // AUTO probing stays a detection operation and always releases power.
    for(unsigned mode=0;mode<3;++mode){
        reset();DistanceSensorModule m;
        m.mb7388Driver.ok=mode==0;m.uartDriver.ok=mode!=2;
        assert(m.scheduleSample(Job::AUTO_PROBE));clockMs=9000;m.serviceSample(clockMs);
        assert(!rail && !m.sampleBusy());
        assert(m.activeType==(mode==0?DistanceSensorType::MB7388:mode==1?DistanceSensorType::UART_GENERIC:DistanceSensorType::NONE));
    }
    // Warm-up and burst remain correct across the 32-bit millisecond wrap.
    reset(UINT32_MAX-4000);DistanceSensorModule wrap;const auto start=clockMs;
    assert(wrap.scheduleSample(Job::READ));clockMs=start+7999;wrap.serviceSample(clockMs);
    assert(rail && wrap.mb7388Driver.reads==0);
    ++clockMs;for(unsigned i=0;i<10;++i){wrap.serviceSample(clockMs);clockMs+=80;}
    assert(!rail && wrap.mb7388Driver.reads==10);
    reset();DistanceSensorModule cancel;assert(cancel.scheduleSample(Job::READ));clockMs=9000;cancel.serviceSample(clockMs);cancel.cancelSample();
    assert(!rail && !cancel.sampleBusy() && cancel.mb7388Driver.reads==1 && cancel.telemetry==0);
    reset();DistanceSensorModule missing;missing.activeDriver=nullptr;
    assert(!missing.scheduleSample(Job::READ) && !rail && powerOns==0);
    puts("PASS: production ten-sample water jobs: warmup, median rounding, outliers, live stage, retries, partial failures, AUTO, cancellation and rollover");
}
"""
)
with tempfile.TemporaryDirectory(prefix="water-jobs-") as directory:
    cpp = Path(directory) / "water_jobs.cpp"
    executable = Path(directory) / "water_jobs"
    cpp.write_text(program)
    subprocess.run(
        [
            "g++",
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            str(cpp),
            "-o",
            str(executable),
        ],
        check=True,
    )
    subprocess.run([str(executable)], check=True)
