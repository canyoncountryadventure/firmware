"""Gateway V5 must decode both deployed MX2001 V1 and new V2 wire formats."""
from pathlib import Path
import struct

root = Path(__file__).resolve().parents[2]
gateway = (root / "src/modules/HoboHttpGatewayModule.cpp").read_text()
sensor = (root / "src/modules/HoboBleSensorModule.cpp").read_text()

for clause in (
    "(n == 19 || n == 21)",
    "payload[2] == 1 && n == 19",
    "payload[2] == 2 && n == 21",
    "stageTenths / 10.0f",
    "stageHundredths / 100.0f",
    "temperatureHundredthsF / 100.0f",
    "payload[20]",
    "payload[14], payload[15], payload[16]",
    "String(job.waterLevelFt, 3)",
    "String(job.temperatureF, 3)",
):
    assert clause in gateway, clause
assert "payload[2] = 2;" in sensor
assert "packet->decoded.payload.size = 21;" in sensor
assert "latestStageFeet * 100.0f" in sensor
assert "latestTemperatureF * 100.0f" in sensor

mac = bytes.fromhex("AABBCCDDEEFF")
v1 = struct.pack("<2sBBHhhH6sb", b"MX", 1, 3, 42, 123, 688, 0x1122, mac, -52)
v2 = struct.pack("<2sBBHihH6sb", b"MX", 2, 3, 42, 1234, 6888, 0x1122, mac, -52)
assert len(v1) == 19 and len(v2) == 21
assert struct.unpack_from("<h", v1, 6)[0] / 10 == 12.3
assert struct.unpack_from("<i", v2, 6)[0] / 100 == 12.34
assert struct.unpack_from("<h", v2, 10)[0] / 100 == 68.88
assert v1[12:18] == v2[14:20] == mac
print("PASS: Heltec V5 signed V1 legacy and V2 hundredths MX2001 decoder contract")
