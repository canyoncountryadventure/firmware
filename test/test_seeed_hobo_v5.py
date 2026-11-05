"""Guard the Seeed V5.1 HOBO recovery and standard-telemetry contract."""

from pathlib import Path


root = Path(__file__).resolve().parents[1]
source = (
    root
    / "src/modules/Telemetry/HOBOMX2001MX2201MX2203/HOBOMX2001MX2201MX2203Telemetry.cpp"
).read_text()
main_header = (root / "src/main.h").read_text()
main_nrf52 = (root / "src/platform/nrf52/main-nrf52.cpp").read_text()


def block(signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    pos = opening + 1
    while depth:
        depth += (source[pos] == "{") - (source[pos] == "}")
        pos += 1
    return source[start:pos]


run_once = block("int32_t HOBOMX2001MX2201MX2203TelemetryModule::runOnce()")
status = source[source.index("case UniversalState::WAIT_STATUS") : source.index("case UniversalState::READY")]
recovery = block("void triggerBleRecovery(const char *reason)")

# MX2001 automatic data must use the normal telemetry route, not the bespoke
# MX private packet that could be filtered while battery traffic survived.
for token in (
    "auto sendHoboTelemetry =",
    "sendHoboTelemetry(true)",
    "meshtastic_PortNum_TELEMETRY_APP",
    "has_distance = true",
    "latestStageFeet * 304.8f",
):
    assert token in run_once, token
assert "sendMX2001MeasurementPacket" not in run_once

# A missing logger gets active bounded acquisition, but healthy stations retain
# passive low-duty scan. Recovery cannot turn ordinary BLE faults into reboot loops.
for token in (
    "HIGH_DUTY_SCAN_MS = 120UL * 1000UL",
    "Bluefruit.Scanner.setInterval(fast ? 160 : 320, fast ? 80 : 32)",
    "Bluefruit.Scanner.useActiveScan(fast)",
    "scanBurstRequested.exchange(false)",
):
    assert token in source, token
assert "nrf52FieldWatchdogTrip" not in recovery
assert "requestHighDutyScanBurst();" in recovery

# Only a decoded STATUS clears the one-reset guard. A staged WDT reset is
# available after BLE-only recovery, not directly from a transient failure.
for token in (
    "nrf52ScheduleWatchdogReset(3000UL)",
    "hoboHardwareResetAlreadyUsed()",
    "HARDWARE_RESET_GRACE_MS",
):
    assert token in run_once, token
for token in ("lastGoodStatusMs = now;", "clearHoboHardwareResetGuard();"):
    assert token in status, token
assert "bool nrf52ScheduleWatchdogReset(uint32_t delayMs);" in main_header
assert "bool nrf52ScheduleWatchdogReset(uint32_t delayMs)" in main_nrf52
assert "scheduledWatchdogResetAtMs" in main_nrf52

print("PASS: Seeed V5.1 status-proven recovery, active scan bursts, and TELEMETRY_APP MX2001 transport")
