"""Regression checks for independent remote watchdog reset and reboot replay.

These are source-structure tests; actual physical reset behavior still requires
a RAK4631 with HOBO hardware.
"""

from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
platform = (root / "src/platform/nrf52/main-nrf52.cpp").read_text()
recovery = (
    root / "src/modules/Telemetry/HOBOSelfRecovery/HOBOSelfRecovery.cpp"
).read_text()
power = (root / "src/Power.cpp").read_text()
readme = (root / "README.md").read_text()


def body(name, text):
    match = re.search(r"\b" + re.escape(name) + r"\s*\([^)]*\)\s*\{", text)
    assert match, name
    depth, pos = 1, match.end()
    while depth:
        depth += (text[pos] == "{") - (text[pos] == "}")
        pos += 1
    return text[match.start() : pos]


guard = body("nrf52MarkRebootPacket", platform)
schedule = body("nrf52ScheduleWatchdogReset", platform)
loop = body("nrf52Loop", platform)
handler = body("HOBOSelfRecoveryModule::handleReceived", recovery)

assert 'section(".noinit")' in platform and "RebootReplayGuard" in platform
for invariant in (
    "rebootReplayGuard.validation == validation",
    "rebootReplayGuard.sender == sender",
    "rebootReplayGuard.packetId == packetId",
    "return false;",
):
    assert invariant in guard, invariant

assert "remoteWatchdogResetAtMs = millis() + delayMs" in schedule
assert "nrf52FieldWatchdogTrip()" in loop
assert "if (fieldWatchdogFeedAllowed)" in loop
assert "if (rebootAtMsec == 0)" in handler
assert 'isCommand(payload, payloadSize, "SOFT REBOOT")' in handler
assert 'isCommand(payload, payloadSize, "RECOVER")' in handler
assert 'isCommand(payload, payloadSize, "REBOOT")' in handler
assert handler.count("nrf52MarkRebootPacket(mp.from, mp.id)") == 2
assert "nrf52ScheduleWatchdogReset(3000UL)" in handler
assert handler.index('"SOFT REBOOT"') < handler.index('"RECOVER"')
assert "sd_softdevice_disable()" in body("Power::reboot", power)
assert "DFU" in recovery
assert "HFCLK DIAG" not in recovery
assert "optstr(APP_VERSION),platformName(),optstr(APP_ENV)" in re.sub(
    r"\s+", "", handler
)
assert "watchdog-origin hardware reset" in readme
assert "hobo_wake=%u hobo_hf=%u hf_rc=%lu" in handler
assert "nrf52HoboHandshakeAwake()" in handler
assert "nrf52HoboHandshakeHfclkRunning()" in handler
assert "nrf52HoboHandshakeHfclkResult()" in handler
wake = body("nrf52SetHoboHandshakeAwake", platform)
assert "sd_clock_hfclk_request()" in wake
assert "sd_clock_hfclk_release()" in wake
assert "hoboHandshakeHfclkRequested.exchange(false)" in wake
assert "rebootReplayGuard" not in body("Power::reboot", power)
print(
    "PASS: watchdog-command reset, retained packet-ID replay guard, "
    "conventional reboot/DFU separation, no sensor or settings changes"
)
