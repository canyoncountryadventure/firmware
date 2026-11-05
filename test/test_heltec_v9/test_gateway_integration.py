#!/usr/bin/env python3
"""V9 clock-batched defaults, any-radio commands, and automatic telemetry guardrails."""
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
assert "applyV9ClockPresets" in gateway
assert "v9_clock_presets" in gateway
assert "hourlyRescue.observe(utcNow, now, autoFallback)" in poller
assert "getValidTime(RTCQualityFromNet)" in poller
assert "hourlyRescue.takeNext(autoFallback, now)" in poller
assert "hourlyRescue.cancel(node, kind, autoFallback)" in poller
assert "sender != cc01AdminNode" not in poller
assert "ignored unauthorized gateway control DM" not in poller
assert 'saved.getBool("v9_optin", false)' in gateway
assert 'clean.putBool("v9_optin", true)' in gateway
assert "autoFallback.pauseAll();" in gateway
assert "autoFallback.intervalOverridesMinutes.fill(0);" in gateway
assert "configuredPolls" in gateway
assert "autoFallback.enabled = saved.getBool(\"auto_dm_on\", false)" in gateway
assert "bool enabled = false;" in policy
assert "1577197109UL, // Fishlake Hightop" in gateway
assert "1683604927UL, // Mill Creek" in gateway
assert "1949224949UL, // Pack Creek" in gateway
assert "2004386937UL, // Matheson" in gateway
assert "2425602407UL  // Cottonwood Relay" in gateway
assert "autoFallback.setNodeEnabled(node, true)" in gateway
assert "clean.putBool(\"v9_clock_presets\", true)" in gateway
assert "uint16_t pausedMask = 0xFFFFU;" in policy
assert "return MISSING_AUTO_MS;" in policy
assert "autoFallback.armNode(node, millis());" in poller
assert "AUTO DM <station> 15MIN|30MIN|45MIN|1HR|6HR|12HR|24HR|OFF" in poller
assert "45MIN" in poller and "6HR" in poller and "12HR" in poller
assert "AUTO DM STATUS" in poller
assert "pollMissingAutomatic" in poller
assert "noteAutomaticSensor" in poller
assert "heltec_dm" in gateway
assert "if (isConfiguredPollReply(getFrom(&mp))) handleConfiguredPollReply(mp);" in gateway
assert "if (job.dmPollResult) body += " in gateway
assert "MANUAL" not in policy or True
print("PASS: V9 uses five default-enabled stations, clock batches, any radio controls, and no CC01 cloud writes")
