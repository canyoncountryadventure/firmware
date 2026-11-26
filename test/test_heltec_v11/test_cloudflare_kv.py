"""V11 Cloudflare KV firmware path and loader preservation contracts."""
from pathlib import Path
root=Path(__file__).resolve().parents[2]
cloud=(root/"src/modules/CcaCloudRemote.cpp").read_text()
stage=(root/"src/modules/CcaRemoteOtaStage.cpp").read_text()
worker=(root/"cloudflare/cces-mesh-remote/worker.js").read_text()
assert "heltec-gateway-v11" in cloud
assert "HELTEC-GATEWAY-V11" in cloud
for src in [cloud,stage]:
    assert "https://cces-mesh-remote.raspy-lake-a7f4.workers.dev/v10/firmware/" in src
    assert "raw.githubusercontent.com/canyoncountryadventure/firmware/" in src
    assert "github.com/canyoncountryadventure/firmware/" in src
assert "unpackSha(url.substring(cloudPrefix.length()), opaqueKey)" in stage
assert "fw_kv=1" in stage
assert "loaderReady()" in stage
assert "LOADER_RESERVED_BYTES = 0xA0000" in stage
assert "esp_partition_write(loader, LOADER_RESERVED_BYTES + copied" in stage
assert 'String(HOBO_HTTP_GATEWAY_URL)' in (root/"src/modules/HoboHttpGatewayModule.cpp").read_text()
assert "env.FW_BIN.get" in worker
assert "/v10/firmware/" in worker
assert "signedManifest" in worker
print("PASS: V11 KV URL allowlist, signed manifest, staged loader, and V9 legacy routing")
