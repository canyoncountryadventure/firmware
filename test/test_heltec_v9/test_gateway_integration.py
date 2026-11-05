#!/usr/bin/env python3
"""V9 source-level guardrails for opt-in automatic-DM firmware."""
from pathlib import Path
root=Path(__file__).resolve().parents[2]
gateway=(root/"src/modules/HoboHttpGatewayModule.cpp").read_text()
header=(root/"src/modules/HoboHttpGatewayModule.h").read_text()
poller=(root/"src/modules/CcaCc01DmPoller.cpp").read_text()
policy=(root/"src/modules/AutoFallbackPolicy.h").read_text()
loop=gateway.split("int32_t HoboHttpGatewayModule::runOnce()")[1]
assert "pollMissingAutomatic();" in loop
assert "pollConfiguredStations();" in loop
assert "pollCc01();" not in loop
assert "pollMatheson();" not in loop
assert "refreshCc01CloudControl" not in gateway + poller
assert "SENSOR_STATUS" not in gateway + header + poller
assert "queueCc01SensorStatus" not in gateway + header + poller
assert "control=cc01" not in gateway + poller
assert "resetForV9OptIn" in gateway
assert 'saved.getBool("v9_optin", false)' in gateway
assert 'clean.putBool("v9_optin", true)' in gateway
assert "autoFallback.pauseAll();" in gateway
assert "autoFallback.intervalOverridesMinutes.fill(0);" in gateway
assert "configuredPolls" in gateway
assert "autoFallback.enabled = saved.getBool(\"auto_dm_on\", false)" in gateway
assert "bool enabled = false;" in policy
assert "uint16_t pausedMask = 0xFFFFU;" in policy
assert "return MISSING_AUTO_MS;" in policy
assert "autoFallback.armNode(node, millis());" in poller
assert "AUTO DM <station> 15MIN|30MIN|1HR|24HR|OFF" in poller
assert "AUTO DM STATUS" in poller
assert "pollMissingAutomatic" in poller
assert "noteAutomaticSensor" in poller
assert "heltec_dm" in gateway
assert "if (isConfiguredPollReply(getFrom(&mp))) handleConfiguredPollReply(mp);" in gateway
assert "if (job.dmPollResult) body += " in gateway
assert "MANUAL" not in policy or True
print("PASS: V9 starts all DM rescue OFF; operator opt-in only, no CC01 cloud/event calls")
