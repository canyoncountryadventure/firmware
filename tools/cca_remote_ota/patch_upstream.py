#!/usr/bin/env python3
"""Apply the CCA remote-stage extension to a pinned Meshtastic unified OTA checkout."""
from pathlib import Path
import shutil
import sys
upstream = Path(sys.argv[1]).resolve()
overlay = Path(__file__).resolve().parent
main = upstream / "src" / "main.cpp"
cmake = upstream / "src" / "CMakeLists.txt"
old = main.read_text()
needle = '    nvs_read_config(&config);'
assert old.count(needle) == 1, "Loader entrypoint changed; review upstream"
assert old.count('#include "nvs_config.h"') == 1
old = old.replace('#include "nvs_config.h"', '#include "nvs_config.h"\n#include "cca_remote_stage.h"')
old = old.replace(needle, needle + '\n\n    // CCA signed/staged OTA install: no internet or local TCP host required.\n    // On failure, return to the unmodified Meshtastic Wi-Fi/BLE loader.\n    cca_remote_stage_install();')
main.write_text(old)
text = cmake.read_text()
needle = '    "ota_processor.cpp"'
assert text.count(needle) == 1, "OTA build configuration changed"
text = text.replace(needle, needle + '\n    "cca_remote_stage.cpp"')
cmake.write_text(text)
for path in ("cca_remote_stage.h", "cca_remote_stage.cpp"):
    shutil.copy2(overlay/path, upstream/"src"/path)
# App descriptor is checked by the V10 staging code before touching OTA_1.
(upstream/"version.txt").write_text("CCA_PULL_1\n")
# Upstream references esp-nimble-cpp but clean CI checkouts do not vendor it.
# Pin the managed ESP-IDF component so Bluetooth recovery is retained.
(upstream/"src"/"idf_component.yml").write_text(
    'dependencies:\n'
    '  h2zero/esp-nimble-cpp:\n'
    '    version: "2.5.0"\n'
)

print("CCA remote OTA loader overlay applied; manual TCP/3232 retained")
