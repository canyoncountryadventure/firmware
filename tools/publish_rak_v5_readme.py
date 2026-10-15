#!/usr/bin/env python3
"""Publish V5 README links only after validating the downloaded release set."""

from pathlib import Path
import hashlib
from datetime import datetime, timezone
import os
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
folder = root / "downloads/RAK-HOBO-v5-Audit-Repair"
source = os.environ["GITHUB_SHA"]
branch = os.environ["GITHUB_REF_NAME"]
assert branch == "RAK-Water-Distance-HOBO-v5"
run_url = f"https://github.com/{os.environ['GITHUB_REPOSITORY']}/actions/runs/{os.environ['GITHUB_RUN_ID']}"
files = []
rows = []
base = f"https://raw.githubusercontent.com/canyoncountryadventure/firmware/{branch}/downloads/RAK-HOBO-v5-Audit-Repair"
for sensor in ("SEN0313", "MB7388"):
    field = folder / f"RAK-HOBO-v5-Audit-Repair-{sensor}.uf2"
    ota = folder / f"RAK-HOBO-v5-Audit-Repair-{sensor}-OTA.zip"
    scout = folder / f"Drone-RAK-HOBO-v5-Audit-Repair-{sensor}.uf2"
    manifest = folder / (scout.name + ".txt")
    for artifact in (field, ota, scout, manifest):
        assert artifact.is_file() and artifact.stat().st_size > 0, artifact
    subprocess.run(
        [sys.executable, str(root / "bin/verify_rak_v5.py"), str(field), source],
        check=True,
    )
    subprocess.run(
        [
            sys.executable,
            str(root / "bin/verify_rak_audit_package.py"),
            str(field),
            str(ota),
        ],
        check=True,
    )
    text = manifest.read_text()
    assert f"Target source commit: {source}" in text
    assert (
        "Scout implementation commit: 37306158a64266fd05dfcd3bf01c5b47d0dc35fa" in text
    )
    for artifact in (scout, ota):
        digest = hashlib.sha256(artifact.read_bytes()).hexdigest()
        assert f"{digest}  export/{artifact.name}" in text, artifact
    files.extend((field, ota, scout))
    rows.append(
        f"| {sensor} | [Field UF2]({base}/{field.name}) | [OTA ZIP]({base}/{ota.name}) | [Drone UF2]({base}/{scout.name}) | [Manifest]({base}/{manifest.name}) |"
    )
checksums = "".join(
    f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n" for p in sorted(files)
)
(folder / "SHA256SUMS.txt").write_text(checksums)
(folder / "BUILD.txt").write_text(
    f"Source commit: {source}\nWorkflow: {run_url}\nScout implementation: 37306158a64266fd05dfcd3bf01c5b47d0dc35fa\n"
)
release = f"""<!-- V5_RELEASE_START -->
Verified firmware: **Meshtastic 2.7.26.{source[:7]}**, source [`{source[:7]}`](https://github.com/canyoncountryadventure/firmware/commit/{source}), built {datetime.now(timezone.utc).strftime('%B %d, %Y')}. Both field flavors and their exact matched autonomous Scouts passed [build/package verification]({run_url}). Selected package: nonblocking **8 s** sensor settling, passive **10%** HOBO scan, bounded CPU wake/HFCLK requests released at the first valid STATUS baseline, and preserved saved settings. `VERSION` reports this exact build and target. [Operating choices and MX2001 evidence](docs/diagnostic-8/V5-FINAL-AUDIT.md).

| Distance sensor | Monitoring station — USB | Field application — BLE DFU | Separate drone radio | Scout pairing |
| --- | --- | --- | --- | --- |
{chr(10).join(rows)}

[SHA-256 checksums]({base}/SHA256SUMS.txt) · [Build source and provenance]({base}/BUILD.txt) · [Flashing/rollback](docs/diagnostic-8/FLASHING.md)
<!-- V5_RELEASE_END -->"""
p = root / "README.md"
text = p.read_text()
updated, count = re.subn(
    r"<!-- V5_RELEASE_START -->.*?<!-- V5_RELEASE_END -->",
    lambda _: release,
    text,
    flags=re.S,
)
assert count == 1
p.write_text(updated)
validation = root / "docs/diagnostic-8/VALIDATION.md"
text = validation.read_text()
section = f"""<!-- CURRENT_CI_START -->
## Current V5 release verification

Source `{source}`; [successful workflow]({run_url}). The pre-idle bounded handshake-wake lifecycle tests, remote wake-state diagnostic checks, BLE fault tests, watchdog-reset command regressions, source warm-up invariants, compiled production water-job scheduling regressions and all 20 preservation checks passed. Both RAK4631 field environments and both autonomous Scout images compiled successfully. UF2 chip/address/source checks, application-only ZIP integrity/DAT CRC/UF2 equivalence, exact embedded Scout application and DAT, and both LZ4 decoders passed. Publication rechecked the field packages and matched Scout manifest hashes before creating the front-page links. Download checksums and source are in `downloads/RAK-HOBO-v5-Audit-Repair`.

This CI run did not test physical RAK/HOBO or airborne transfer. The prior October 15 log confirms an MX2001 STATUS response on baseline `cb632f6`; this finalization retains that baseline's water/BLE/power behavior and changes only the runtime label and VERSION identification. Repeated battery-only and airborne checks remain open. See [final audit](V5-FINAL-AUDIT.md). Earlier local and unavailable native-suite results below remain distinct from this completed CI build.
<!-- CURRENT_CI_END -->

"""
if "<!-- CURRENT_CI_START -->" in text:
    text = re.sub(
        r"<!-- CURRENT_CI_START -->.*?<!-- CURRENT_CI_END -->\n*", "", text, flags=re.S
    )
text = text.replace(
    "# Diagnostic 8 validation\n", "# Diagnostic 8 validation\n\n" + section, 1
)
text = text.replace(
    "Matched autonomous Scout builds and final CI package publication are in progress.",
    "Matched autonomous Scout builds and CI package publication completed; see the current V5 release verification above.",
)
validation.write_text(text)
print(
    "PASS: complete matched release validated; V5 front-page links and validation record updated"
)
