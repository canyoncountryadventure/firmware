#!/usr/bin/env python3
"""Gate V6 public links behind source/field/OTA/Scout verification."""
import hashlib
import os
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
folder = root / "downloads/RAK-HOBO-v6"
source = os.environ["GITHUB_SHA"]
assert os.environ["GITHUB_REF_NAME"] == "RAK-Water-Distance-HOBO-v6"
run_url = f"https://github.com/{os.environ['GITHUB_REPOSITORY']}/actions/runs/{os.environ['GITHUB_RUN_ID']}"
base = f"https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v6/downloads/RAK-HOBO-v6"

rows, binaries, dates = [], [], []
for sensor in ("SEN0313", "MB7388"):
    build_date = (folder / f"BUILD-DATE-{sensor}.txt").read_text().strip()
    assert re.fullmatch(r"\\d{2}-\\d{2}-\\d{2}", build_date), build_date
    dates.append(build_date)
    field = folder / f"field-rak-hobo-distance-{build_date}-{sensor}.uf2"
    ota = folder / f"field-rak-hobo-distance-{build_date}-{sensor}-OTA.zip"
    scout = folder / f"drone-rak-hobo-distance-{build_date}-{sensor}.uf2"
    manifest = folder / (scout.name + ".txt")
    for path in (field, ota, scout, manifest):
        assert path.is_file() and path.stat().st_size > 0, path
    subprocess.run([sys.executable, str(root / "bin/verify_rak_v5.py"),
                    str(field), source], check=True)
    subprocess.run([sys.executable, str(root / "bin/verify_rak_audit_package.py"),
                    str(field), str(ota)], check=True)
    scout_manifest = manifest.read_text()
    assert f"Target source commit: {source}" in scout_manifest
    assert "Scout implementation commit: 37306158a64266fd05dfcd3bf01c5b47d0dc35fa" in scout_manifest
    for path in (scout, ota):
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        assert f"{digest}  export/{path.name}" in scout_manifest, path
    binaries.extend((field, ota, scout))
    rows.append(f"| {sensor} | [Field UF2]({base}/{field.name}) | "
                f"[BLE OTA ZIP]({base}/{ota.name}) | [Drone Scout UF2]({base}/{scout.name}) | "
                f"[Matched manifest]({base}/{manifest.name}) |")

assert dates[0] == dates[1], "Do not mix build dates"
(folder / "SHA256SUMS.txt").write_text("".join(
    f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\\n"
    for p in sorted(binaries)))
(folder / "BUILD.txt").write_text(
    f"Source commit: {source}\\nCI run: {run_url}\\nBuild date (America/Denver): {dates[0]}\\n"
    "Hardware: RAK4631/RAK19007\\n"
    "Field images: application-only; preserves configuration and calibration\\n"
    "Matching Scout implementation: 37306158a64266fd05dfcd3bf01c5b47d0dc35fa\\n"
    "Field and Scout contents verified; this is not a physical/radio test\\n")
release = f"""<!-- V6_RELEASE_START -->
### Verified October {int(dates[0][3:5])}, 2026 firmware

Build **{source[:7]}** ([source commit](https://github.com/canyoncountryadventure/firmware/commit/{source})). Both field sensor flavors and both matching Scout flasher images passed [GitHub CI]({run_url}), including application/OTA/UF2/embedded Scout verification.

| Sensor | Field station USB | Field station BLE | Separate drone flasher | Matching record |
| --- | --- | --- | --- | --- |
{chr(10).join(rows)}

[SHA-256 checksums]({base}/SHA256SUMS.txt) · [Build provenance]({base}/BUILD.txt)

**Field UF2 is for the deployed station. Drone UF2 is for the separate autonomous Scout/RAK4631.** Use the matched Scout image, not an older drone image. No erase or factory reset for application-only updates. Physical startup, reconnection and drone transfer still require field tests.
<!-- V6_RELEASE_END -->"""
p = root / "README.md"
text = p.read_text()
updated, count = re.subn(r"<!-- V6_RELEASE_START -->.*?<!-- V6_RELEASE_END -->",
                         lambda _: release, text, flags=re.S)
assert count == 1, "V6 marker must occur once"
p.write_text(updated)
print("PASS: V6 release images, Scout payloads, provenance, SHA-256 and README validated")
