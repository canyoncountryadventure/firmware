"""Source-level safety invariants for asynchronous Pololu and water sampling.

Physical RAK + HOBO validation is still required. No simulated RF success claim.
"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
source = (root / "src/modules/Telemetry/DistanceSensor/DistanceSensorModule.cpp").read_text()
header = (root / "src/modules/Telemetry/DistanceSensor/DistanceSensorModule.h").read_text()

assert "delay(SENSOR_POWER_SETTLE_MS)" not in source
assert "while ((millis() - warmupStarted) < SENSOR_POWER_SETTLE_MS)" not in source
assert "SampleJob sampleJob = SampleJob::NONE" in header
assert "samplePowerOnMs + SENSOR_POWER_SETTLE_MS" in source
assert "static_cast<int32_t>(now - nextSampleMs) < 0" in source
assert "serviceSample(now);" in source
assert "return sampleBusy() ? 100 : 1000;" in source
assert "if (!sampleBusy())\n            setDistanceSensorPower(false);" in source
assert "sampleJob = SampleJob::NONE;\n    setDistanceSensorPower(false);" in source
for job in ("SCHEDULED", "STATUS", "READ", "RAW", "VERIFY", "CALIBRATE", "TELEMETRY_NOW", "AUTO_PROBE"):
    assert f"SampleJob::{job}" in source, job
assert "readDistance();" in source[source.index("void DistanceSensorModule::serviceSample"):source.index("bool DistanceSensorModule::sendDistanceTelemetry")]
assert "readDistance();" not in source[source.index("ProcessMessage DistanceSensorModule::handleReceived"):source.index("int32_t DistanceSensorModule::runOnce")]
assert "std::sort(sampleValues, sampleValues + sampleCount)" in source
assert "finishCalibration(knownStage, requester, channel, median, low, high)" in source
assert "sampleJob != SampleJob::NONE || activeDriver == nullptr" in source
assert "if (sampleBusy() && strncmp(command, \"CAL \", 4) == 0" in source
assert "setDistanceSensorPower(true);" in source
print("PASS: nonblocking 8s sensor warmup, single power window, deferred replies, safe calibration and AUTO probing")
