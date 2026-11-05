"""Guard the V5.1 HOBO progress/recovery and standard telemetry contract.

This is intentionally a source-level contract test. Physical BLE/HOBO behavior
still needs field validation on a RAK4631 or Seeed node.
"""

from pathlib import Path


root = Path(__file__).resolve().parents[2]
source = (
    root
    / "src/modules/Telemetry/HOBOMX2001MX2201MX2203/HOBOMX2001MX2201MX2203Telemetry.cpp"
).read_text()
main = (root / "src/main.cpp").read_text()


def function_block(signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    pos = opening + 1
    while depth:
        depth += (source[pos] == "{") - (source[pos] == "}")
        pos += 1
    return source[start:pos]


run_once = function_block("int32_t HOBOMX2001MX2201MX2203TelemetryModule::runOnce()")
status_block = source[source.index("case UniversalState::WAIT_STATUS") : source.index("case UniversalState::READY")]

# MX2001 now rides the same normal TELEMETRY_APP transport as battery data.
assert "auto sendHoboTelemetry =" in run_once
assert "sendHoboTelemetry(true)" in run_once
assert "sendHoboTelemetry(false)" in run_once
assert "meshtastic_PortNum_TELEMETRY_APP" in run_once
assert "has_distance = true" in run_once
assert "latestStageFeet * 304.8f" in run_once
assert "sendMX2001MeasurementPacket" not in run_once

# Only a decoded STATUS reply clears stale-link recovery. Scanner/radio state
# or ordinary telemetry must not reset the health clock.
for invariant in (
    "FIRST_DEEP_BLE_RECOVERY_MS",
    "HARDWARE_RESET_GRACE_MS",
    "BLE_RECOVERY_FILE_PATH",
    "BLE RESET AUTO",
    "HOBO RESTART",
    "nrf52ScheduleWatchdogReset(3000UL)",
    "hoboHardwareResetAlreadyUsed()",
    "serviceDisconnectedScanner(now)",
    "HIGH_DUTY_SCAN_MS = 120UL * 1000UL",
    "Bluefruit.Scanner.useActiveScan(highDutyScanActive)",
):
    assert invariant in source, invariant
for invariant in (
    "lastGoodStatusMs = now;",
    "bleHealthStartedMs = now;",
    "clearHoboHardwareResetGuard();",
):
    assert invariant in status_block, invariant

# Whole-node maintenance stays staggered so all relays do not reset together.
assert "fieldRebootStaggerMs" in main
assert "scheduled 12h reboot" in main

print("PASS: V5.1 STATUS-proven recovery, bounded whole-node reset, and TELEMETRY_APP MX2001 transport")
