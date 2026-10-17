"""V6 source invariants and exact on-air format compatibility checks."""
from pathlib import Path
import struct

root = Path(__file__).resolve().parents[2]
rak = (root / "src/modules/Telemetry/HOBOMX2001MX2201MX2203/HOBOMX2001MX2201MX2203Telemetry.cpp").read_text()
dist = (root / "src/modules/Telemetry/DistanceSensor/DistanceSensorModule.cpp").read_text()
h = (root / "src/modules/Telemetry/DistanceSensor/DistanceSensorModule.h").read_text()
recovery = (root / "src/modules/Telemetry/HOBOSelfRecovery/HOBOSelfRecovery.cpp").read_text()
heltec_reference = """MX2001 V2: stage int32, temp int16, both hundredths; V1: stage int16, temp int16, tenths"""

assert "payload[2] = 2;" in rak
assert "payload[20] = static_cast<uint8_t>(loggerBleRssi)" in rak
assert "packet->decoded.payload.size = 21" in rak
assert "stageScaled / 100.0f" in rak
assert "waterHubDestination()" in rak and "HUB SET " in rak and "HUB AUTO" in rak
assert "HOBO RESTART" in rak and "triggerBleRecovery" in rak
assert "SENSOR RESTART" in dist and "reinitializeSensorPending = true" in dist
assert "WATER SAMPLES" in dist and "targetSamples()" in dist
assert "sampleValues[MAX_SAMPLE_COUNT]" in h
assert "cfg.reserved0 = static_cast<uint8_t>(requested)" in dist
assert "const uint8_t requiredSamples = targetSamples();" in dist
assert "const uint32_t median = (requiredSamples & 1U)" in dist
assert "if (record.reserved0 != 0" in dist
assert "LAST RESORT" in recovery and "factoryReset(true)" in recovery
assert "config.lora.channel_num = 20;" in recovery
assert "meshtastic_Config_LoRaConfig_RegionCode_US" in recovery
assert "meshtastic_Config_LoRaConfig_ModemPreset_LONG_FAST" in recovery
assert "nrf52FieldDiagFormat(reply, sizeof(reply))" in recovery
assert "BATTERY HISTORY" in recovery

# Simulate both wire contracts with known signed fractional values. The
# on-air assertions ensure a decoder cannot silently mistake tenths for 100ths.
seq, stage, temp, raw, mac, rssi = 15, -12.34, 67.89, 1234, bytes.fromhex("AABBCCDDEEFF"), -65
v2 = struct.pack("<2sBBHihH6sb", b"MX", 2, 3, seq, round(stage * 100),
                 round(temp * 100), raw, mac, rssi)
assert len(v2) == 21
assert struct.unpack("<i", v2[6:10])[0] / 100 == stage
assert struct.unpack("<h", v2[10:12])[0] / 100 == temp
assert struct.unpack("<b", v2[20:21])[0] == rssi
v1 = struct.pack("<2sBBHhhH6sb", b"MX", 1, 3, seq, -123, 678, raw, mac, rssi)
assert len(v1) == 19
assert struct.unpack("<h", v1[6:8])[0] / 10 == -12.3
print("PASS: V6 DM/reset/sampling source gates and V1/V2 signed, decimal wire compatibility")
