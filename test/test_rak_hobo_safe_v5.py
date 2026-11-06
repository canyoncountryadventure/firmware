"""Static contract checks for the dated RAK HOBO Safe V5.1 release."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (
    ROOT
    / "src/modules/Telemetry/HOBOMX2001MX2201MX2203/HOBOMX2001MX2201MX2203Telemetry.cpp"
).read_text()
MAIN = (ROOT / "src/main.cpp").read_text()
MAIN_H = (ROOT / "src/main.h").read_text()
NRF52 = (ROOT / "src/platform/nrf52/main-nrf52.cpp").read_text()


def block(signature: str) -> str:
    start = SOURCE.index(signature)
    opening = SOURCE.index("{", start)
    depth = 1
    pos = opening + 1
    while depth:
        depth += (SOURCE[pos] == "{") - (SOURCE[pos] == "}")
        pos += 1
    return SOURCE[start:pos]


run_once = block("int32_t HOBOMX2001MX2201MX2203TelemetryModule::runOnce()")
status_block = SOURCE[SOURCE.index("case UniversalState::WAIT_STATUS") : SOURCE.index("case UniversalState::READY")]

for token in (
    "HANDSHAKE_WAKE_COOLDOWN_MS = 30000UL",
    "HIGH_DUTY_SCAN_MS = 120UL * 1000UL",
    "Bluefruit.Scanner.useActiveScan(highDutyScanActive)",
    "nrf52ScheduleWatchdogReset(3000UL)",
    "hoboHardwareResetAlreadyUsed()",
    "BLE RESET AUTO",
    "HOBO RESTART",
    "AUTO STATUS",
    "#if defined(HAS_RGB_LED) && !defined(RAK_4631)",
):
    assert token in SOURCE, token

for token in (
    "sendHoboTelemetry(true)",
    "sendHoboTelemetry(false)",
    "meshtastic_PortNum_TELEMETRY_APP",
    "has_distance = true",
    "latestStageFeet * 304.8f",
):
    assert token in run_once, token

assert "sendMX2001MeasurementPacket" not in run_once
for token in ("lastGoodStatusMs = now;", "bleHealthStartedMs = now;", "clearHoboHardwareResetGuard();"):
    assert token in status_block, token

for token in (
    "nrf52SetHoboHandshakeAwake",
    "nrf52ScheduleWatchdogReset",
):
    assert token in MAIN_H, token
    assert token in NRF52, token

assert "fieldRebootStaggerMs" in MAIN
print("PASS: RAK HOBO Safe V5.1 BLE recovery and standard telemetry contracts")
