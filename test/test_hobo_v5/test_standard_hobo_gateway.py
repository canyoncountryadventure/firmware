"""Guard the laptop bridge's V5.1 standard MX2001 telemetry compatibility."""

from pathlib import Path


root = Path(__file__).resolve().parents[2]
source = (root / "tools/hobo_cloud_gateway.py").read_text()

for token in (
    "STANDARD_MX2001_NODE_IDS = {1949224949, 1683604927, 2004386937}",
    "EnvironmentMetrics.distance (millimetres)",
    '"type": "mx2001"',
    '"source": "telemetry_app"',
    "stage_ft = distance_mm / 304.8",
    "math.isfinite(distance_mm)",
):
    assert token in source, token

print("PASS: laptop bridge recognizes guarded V5.1 MX2001 TELEMETRY_APP observations")
