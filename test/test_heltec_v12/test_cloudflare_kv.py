"""V12 Cloudflare KV firmware path and loader preservation contracts."""
from pathlib import Path
root=Path(__file__).resolve().parents[2]
cloud=(root/"src/modules/CcaCloudRemote.cpp").read_text()
stage=(root/"src/modules/CcaRemoteOtaStage.cpp").read_text()
worker=(root/"cloudflare/cces-mesh-remote/worker.js").read_text()
assert "heltec-gateway-v12" in cloud
assert "HELTEC-GATEWAY-V12" in cloud
for src in [cloud,stage]:
    assert "https://cces-mesh-remote.raspy-lake-a7f4.workers.dev/v10/firmware/" in src
    assert "raw.githubusercontent.com/canyoncountryadventure/firmware/" in src
    assert "github.com/canyoncountryadventure/firmware/" in src
assert "unpackSha(url.substring(cloudPrefix.length()), opaqueKey)" in stage
assert "fw_kv=1" in stage
assert "last_config_error" in cloud
assert "tls_largest_heap_block" in cloud
assert "http.setConnectTimeout(12000)" in cloud
assert "const uint32_t retryMs = 30000UL" in cloud
assert "lastHealthCode" in cloud
assert "loaderReady()" in stage
assert "LOADER_RESERVED_BYTES = 0xA0000" in stage
assert "esp_partition_write(loader, LOADER_RESERVED_BYTES + copied" in stage
assert 'String(HOBO_HTTP_GATEWAY_URL)' in (root/"src/modules/HoboHttpGatewayModule.cpp").read_text()
assert "env.FW_BIN.get" in worker
assert "fwMatch=url.pathname.match" in worker and "heltec-v10/" in worker
assert "signedManifest" in worker
print("PASS: V12 KV URL allowlist, signed manifest, staged loader, and V9 legacy routing")

gateway=(root/"src/modules/HoboHttpGatewayModule.cpp").read_text()
gateway_header=(root/"src/modules/HoboHttpGatewayModule.h").read_text()
dashboard=(root/"cloudflare/cces-monitor/firmware-manager-2026-11-25.js").read_text()
assert "autoDmSnapshotJson()" in gateway
assert "autoDmSnapshotJson() const;" in gateway_header
for key in ["schema", "global_enabled", "minimum_missing_minutes", "clock_synchronized", "watches", "configured_polls", "last_auto_age_seconds", "last_request_age_seconds"]:
    assert key in gateway, key
for marker in ["reset_reason_code","reset_diagnostic","auto_dm_state","esp32FieldDiagFormat","HELTEC-GATEWAY-V12"]:
    assert marker in cloud,marker
for marker in ["mesh_gateway_reset_diag","mesh_gateway_dm_state","d.auto_dm_state"]:
    assert marker in worker,marker
for marker in ['id="dm-view"',"'/api/mesh/dm'","mesh_gateway_dm_actions","dmWatched"]:
    assert marker in dashboard,marker
print("PASS V12 reset reasons, live DM snapshots, authenticated storage and owner UI")

assert "last_health_http_status" in cloud
assert "last_config_error" in cloud
assert "ESP.restart()" in stage
assert "last_recovery_reason" in (root/"src/platform/esp32/main-esp32.cpp").read_text()
