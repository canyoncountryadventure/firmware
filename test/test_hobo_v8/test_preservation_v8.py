"""V8 preservation checks: keep the V5/V7 immutable sections while adding A08.

The original preservation.json remains untouched, including baseline hashes for
entire driver files now intentionally extended with the third sensor.
"""
from pathlib import Path
import hashlib
import json
import re

root = Path(__file__).resolve().parents[2]
manifest = json.loads((root / "test/test_hobo_v5/preservation.json").read_text())
intentionally_extended = {
    "src/modules/Telemetry/DistanceSensor/DistanceSensorDrivers.cpp",
    "src/modules/Telemetry/DistanceSensor/DistanceSensorDrivers.h",
    "variants/nrf52840/rak4631/platformio.ini",
}
count = 0
for item in manifest["checks"]:
    if item["path"] in intentionally_extended:
        continue
    value = (root / item["path"]).read_text()
    if "signature" in item:
        pattern = re.escape(item["signature"]).replace(r"\ ", r"\s*")
        match = re.search(pattern + r"\s*\{", value)
        assert match, item["signature"]
        pos, depth = value.index("{", match.start()) + 1, 1
        while depth:
            depth += (value[pos] == "{") - (value[pos] == "}")
            pos += 1
        value = value[match.start():pos]
    digest = hashlib.sha256(re.sub(r"\s+", "", value).encode()).hexdigest()
    assert digest == item["sha256"], item
    count += 1

source = (root / "src/modules/Telemetry/DistanceSensor/DistanceSensorDriver.h").read_text()
for declaration in ("AUTO = 0", "SEN0590 = 1", "SEN0311_A02YYUW = 2",
                    "SEN0313_A01NYUB = 3", "UART_GENERIC = 4",
                    "MB7388 = 5", "DYP_A08_CONTROLLED = 6"):
    assert declaration in source, declaration
for label in ("DFRobotUARTDistanceDriver::read()", "MaxBotixMB7388DistanceDriver::read()",
              "DYPA08ControlledDistanceDriver::read()"):
    assert label in (root / "src/modules/Telemetry/DistanceSensor/DistanceSensorDrivers.cpp").read_text()
assert "HUB SET " in (root / manifest["hobo_path"]).read_text()
print(f"PASS V8: {count} unchanged legacy preservation sections and stable sensor IDs")
