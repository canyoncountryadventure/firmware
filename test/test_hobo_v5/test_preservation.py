"""Check audited protocol/storage code against immutable V5 baseline hashes."""

from pathlib import Path
import hashlib
import json
import re

root = Path(__file__).resolve().parents[2]
manifest = json.loads((Path(__file__).parent / "preservation.json").read_text())
for item in manifest["checks"]:
    text = (root / item["path"]).read_text()
    if "signature" in item:
        signature = re.escape(item["signature"]).replace(r"\ ", r"\s*")
        match = re.search(signature + r"\s*\{", text)
        assert match, item["signature"]
        pos, depth = text.index("{", match.start()) + 1, 1
        while depth:
            depth += (text[pos] == "{") - (text[pos] == "}")
            pos += 1
        text = text[match.start() : pos]
    normalized = re.sub(r"\s+", "", text)
    digest = hashlib.sha256(normalized.encode()).hexdigest()
    assert digest == item["sha256"], item
hobo = (root / manifest["hobo_path"]).read_text()
for name, expected in manifest["uuids"].items():
    declaration = re.search(name + r"\[16\]\s*=\s*\{([^}]+)", hobo).group(1)
    assert re.findall(r"0x[0-9A-Fa-f]+", declaration) == expected, name
sensor = (root / manifest["distance_path"]).read_text()
start = sensor.index("DistanceReading DistanceSensorModule::readDistance()")
end = sensor.index("bool DistanceSensorModule::", start)
reading = sensor[start:end]
assert "saveConfig(" not in reading
assert "cfg.flags &=" not in reading
assert "cfg.calibrationRawMm =" not in reading
assert "cfg.calibrationStageMm =" not in reading
print(
    f"PASS: {len(manifest['checks'])} unchanged protocol/storage sections, UUIDs, and no persistent calibration migration during reads"
)
