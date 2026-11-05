"""Contract test for RAK/Seeed V5 standard MX2001 telemetry ingestion."""

from pathlib import Path


root = Path(__file__).resolve().parents[2]
source = (root / "src/modules/HoboHttpGatewayModule.cpp").read_text()
header = (root / "src/modules/HoboHttpGatewayModule.h").read_text()


def block(signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    pos = opening + 1
    while depth:
        depth += (source[pos] == "{") - (source[pos] == "}")
        pos += 1
    return source[start:pos]


enqueue = block("bool HoboHttpGatewayModule::enqueueEnvironment")
serialize = block("String HoboHttpGatewayModule::serializeJob")

# EnvironmentMetrics has no model tag. It must only become an MX2001 record
# when paired distance+temperature comes from the known MX2001 stations.
for token in (
    "isStandardTelemetryMX2001",
    "node == SWELL_NODE || node == MILL_CREEK_NODE || node == MATHESON_NODE",
    "environment.has_distance",
    "std::isfinite(distanceMm)",
    "job.type = JobType::MX2001",
    "job.standardTelemetry = true",
    "job.waterLevelFt = distanceMm / 304.8f",
    "cca_auto_fallback::Kind::Mx2001",
):
    assert token in source if token.startswith("node ==") else token in enqueue, token

# Standard telemetry must preserve the source distinction and never fabricate
# proprietary logger details absent from an EnvironmentMetrics protobuf.
assert "bool standardTelemetry;" in header
assert "telemetry_app" in serialize
assert "if (job.standardTelemetry)" in serialize
assert "Do not invent them." in serialize

print("PASS: V8 ingests guarded MX2001 TELEMETRY_APP stage and preserves source provenance")
