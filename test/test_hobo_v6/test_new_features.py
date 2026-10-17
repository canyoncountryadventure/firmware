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

# Preferred relay: central RAK router covers all outgoing application packets;
# broadcasts use fresh-ID flood fallback while unicast uses ACK-based retries.
relay = (root / "src/modules/Telemetry/WaterRelayTarget.h").read_text()
reliable = (root / "src/mesh/ReliableRouter.cpp").read_text()
reliable_h = (root / "src/mesh/ReliableRouter.h").read_text()
next_hop = (root / "src/mesh/NextHopRouter.cpp").read_text()
assert "water_relay_v6.bin" in relay and "stored.magic" in relay
assert "waterRelayOnceNode() = 0;" in relay
# Router-level preference must affect all local outgoing app traffic,
# not just the individual water and HOBO emitters.
assert "waterRelayPrepareTelemetry(packet)" not in rak
assert "waterRelayPrepareTelemetry(packet)" not in dist
assert "waterRelayPrepareTelemetry(p);" in reliable
assert reliable.index("waterRelayPrepareTelemetry(p);") < reliable.index("if (p->want_ack)"), (
    "DM ACK retry must snapshot preference after selection")
assert "packet->decoded.portnum" in relay
assert "meshtastic_PortNum_ROUTING_APP" in relay and "meshtastic_PortNum_ADMIN_APP" in relay
assert "packet->to != NODENUM_BROADCAST" not in relay, "Unicast DMs must be eligible"
assert "const bool isSensor" in relay and "const uint32_t pending = isSensor" in relay, (
    "RELAY ONCE must not be consumed by its own outgoing command reply")
assert "getNextHop(p->to, p->relay_node)" in next_hop
assert "if (!isFromUs(p) || p->next_hop == NO_NEXT_HOP_PREFERENCE)" in next_hop
assert "FloodingRouter::send(packetPool.allocCopy(*p.packet));" in next_hop, (
    "Last unicast ACK retry must bypass preferred next hop and flood normally")
assert "RELAY PREFER " in rak and "RELAY ONCE " in rak
assert "RELAY AUTO" in rak and "RELAY SHOW" in rak
assert "relayFallbacks[4]" in reliable_h
assert "RELAY_FALLBACK_MS = 25000UL" in reliable_h
assert "p->next_hop != NO_NEXT_HOP_PREFERENCE" in reliable
assert "p->transport_mechanism == meshtastic_MeshPacket_TransportMechanism_TRANSPORT_LORA" in reliable
assert "p->relay_node == entry.expectedRelay" in reliable
assert "fallback->id = generatePacketId();" in reliable
assert "fallback->next_hop = NO_NEXT_HOP_PREFERENCE;" in reliable
assert "FloodingRouter::send(fallback)" in reliable
assert "p->next_hop = NO_NEXT_HOP_PREFERENCE;" in reliable  # no RAM => normal broadcast
assert "meshtastic_PortNum_TRACEROUTE_APP" not in relay, (
    "Traceroutes must be eligible without a special-case opt-out")
assert "p.packet->next_hop = NO_NEXT_HOP_PREFERENCE;" in next_hop, "ACK-based fallback must clear preference"
assert "isFromUs(p) && millis() >= 8000UL" in reliable, "Avoid early FS load"
wrapper = (root / "src/modules/Telemetry/HOBOMX2001MX2201MX2203/HOBOMX2001MX2201MX2203TelemetryRAK.cpp").read_text()
assert "#define SEEED_XIAO_NRF52840_KIT 1" in wrapper
assert '#include "HOBOMX2001MX2201MX2203Telemetry.cpp"' in wrapper
assert '#if defined(ARCH_NRF52) && defined(SEEED_XIAO_NRF52840_KIT)' in rak

print("PASS: V6 relay DM/fallback gates, recovery and V1/V2 decimal wire compatibility")
