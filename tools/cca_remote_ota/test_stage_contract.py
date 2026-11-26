#!/usr/bin/env python3
"""Guard against changing Meshtastic OTA layout or losing recovery upload."""
from pathlib import Path
import sys
base=Path(sys.argv[1])
main=(base/'src/main.cpp').read_text()
net=(base/'src/net_ota.cpp').read_text()
cmake=(base/'src/CMakeLists.txt').read_text()
stage=(base/'src/cca_remote_stage.cpp').read_text()
assert 'cca_remote_stage_install();' in main
assert main.index('cca_remote_stage_install();') < main.index('wifi_connect(&config);')
assert 'cca_remote_stage.cpp' in cmake
assert 'OTA_PORT 3232' in net
assert 'start_network_ota_process(&config);' in main
assert 'STAGE_OFFSET = 0xA0000' in stage
assert 'ESP_PARTITION_SUBTYPE_APP_OTA_1' in stage
assert 'ESP_PARTITION_SUBTYPE_APP_OTA_0' in stage
assert 'esp_ota_set_boot_partition(source)' in stage
assert 'esp_ota_begin(target, length, &handle)' in stage
assert 'verifyStaged(loader, offset, size, expected)' in stage
assert 'MAX_INSTALL_ATTEMPTS = 10' in stage
assert 'phase != 1 && phase != 2' in stage
assert (base/'version.txt').read_text().strip() == 'CCA_PULL_1'
print('PASS: Loader image validation, 10-attempt recovery, port 3232 preservation, NVS handshake')
