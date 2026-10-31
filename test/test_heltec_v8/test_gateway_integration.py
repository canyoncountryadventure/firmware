#!/usr/bin/env python3
"""V8 source-level safety contract; native policy is tested separately in C++."""
from pathlib import Path
root = Path(__file__).resolve().parents[2]
gateway = (root/"src/modules/HoboHttpGatewayModule.cpp").read_text()
poller = (root/"src/modules/CcaCc01DmPoller.cpp").read_text()
header = (root/"src/modules/HoboHttpGatewayModule.h").read_text()
policy = (root/"src/modules/AutoFallbackPolicy.h").read_text()
mainloop = gateway.split("int32_t HoboHttpGatewayModule::runOnce()")[1]
assert "pollMissingAutomatic();" in mainloop
assert "pollConfiguredStations();" in mainloop
assert "pollCc01();" not in mainloop, "Unconditional V7 Cottonwood poll remains"
assert "pollMatheson();" not in mainloop, "Unconditional V7 Matheson poll remains"
assert "noteAutomaticSensor(job.from, cca_auto_fallback::Kind::Mx2001)" in gateway
assert "noteAutomaticSensor(job.from, cca_auto_fallback::Kind::Water)" in gateway
assert "noteAutomaticSensor(job.from, cca_auto_fallback::Kind::Temperature)" in gateway
assert "if (job.stageCalibrated)" in gateway
assert gateway.index("if (isConfiguredPollReply(getFrom(&mp))) handleConfiguredPollReply(mp);") < gateway.index("else if (getFrom(&mp) == CC01_NODE) handleCc01Reply(mp);")
assert "if (job.dmPollResult) body += " in gateway
assert "heltec_dm" in gateway
assert "AUTO_FALLBACK_POLL_SLOT" in poller
assert "pollMissingAutomatic" in poller
assert "if (watch.node == CC01_POLL_NODE" in poller
assert "!cc01WaterEnabled" in poller
assert "poll.mode == mode" in poller or "fallback.mode == mode" in poller
assert "cc01NextConfigMs=now+120000UL" in poller or "cc01NextConfigMs = now + 120000UL" in poller
assert "const uint8_t i = order == 0 ? AUTO_FALLBACK_POLL_SLOT : order - 1;" in poller
assert "REPLY_TIMEOUT_MS = 30000UL" in policy
assert "MISSING_AUTO_MS = 60UL * 60UL * 1000UL" in policy
assert "FAST_MISSING_AUTO_MS = 15UL * 60UL * 1000UL" in policy
assert "static_cast<uint32_t>(intervalMinutes(index)) * 60000UL" in policy
for station in ("1949224949UL", "1683604927UL", "1695962694UL"):
    assert station in policy
assert "CONFIGURED_POLL_SLOTS = CONFIGURED_POLL_PERSISTENT_SLOTS + 2" in header
assert "v8_fallback" in gateway
assert "getFrom(&mp) == PACK_CREEK_POLL_NODE ? 3 : 0" in poller
assert "if (getFrom(&mp) == CC01_POLL_NODE)" in poller
assert "temperatureOnly ? JobType::ENVIRONMENT : JobType::MX2001" in poller
print("PASS: V8 gateway only rescues stale sensor streams; LoRa, NVS, parser and gateway wire paths retained")

assert 'autoFallback.enabled = saved.getBool("auto_dm_on", true)' in gateway
assert 'autoFallback.pausedMask = saved.getUShort("auto_dm_mask", 0)' in gateway
assert 'prefs.putBool("auto_dm_on", autoFallback.enabled)' in poller
assert 'prefs.putUShort("auto_dm_mask", autoFallback.pausedMask)' in poller
assert 'text.startsWith("AUTO DM ")' in poller
assert 'operation == "ONLY"' in poller
assert 'AUTO DM STATUS' in poller
assert 'autoFallback.nodeEnabled(fallback.node)' in poller
assert 'if (!isEnabled(index)) return false;' in policy

assert 'autoFallback.intervalOverridesMinutes.data()' in gateway
assert 'getBytesLength("auto_dm_int")' in gateway
assert 'prefs.putBytes("auto_dm_int"' in poller
assert 'operation == "INTERVAL"' in poller
assert 'AUTO DM INTERVAL <station|ALL> <5..1440|DEFAULT>' in poller
assert 'autoFallback.setNodeIntervalMinutes(selected, minutes)' in poller
assert 'autoFallback.setAllIntervalMinutes(minutes)' in poller
assert 'String(autoFallback.nodeIntervalMinutes(node))' in poller
assert 'intervalMinutes(index)) * 60000UL' in policy
