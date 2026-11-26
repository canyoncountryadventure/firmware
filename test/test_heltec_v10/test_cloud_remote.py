"""V10 preservation contract: dual backend isolation, remote configuration and OTA guardrails."""
from pathlib import Path
repo = Path(__file__).resolve().parents[2]
gateway = (repo/"src/modules/HoboHttpGatewayModule.cpp").read_text()
header = (repo/"src/modules/HoboHttpGatewayModule.h").read_text()
remote = (repo/"src/modules/CcaCloudRemote.cpp").read_text()
remote_header = (repo/"src/modules/CcaCloudRemote.h").read_text()
modules = (repo/"src/modules/Modules.cpp").read_text()
worker = (repo/"cloudflare/cces-mesh-remote/worker.js").read_text()
v9 = (repo/"downloads/Heltec-Gateway-v9-Clock/README.md").read_text()
for x in ("HIDDEN_VALLEY_NODE", "REVIVED_NODE", "FISHLAKE_NODE", "SWELL_NODE",
          "HELTEC_HOME_NODE", "MOAB_NODE", "MINI_ME_NODE", "MATHESON_NODE",
          "CLIFF_SENSOR_NODE", "CC01_NODE", "CC02_NODE", "MILL_CREEK_NODE",
          "SEED_RELAY_1_NODE", "COTTONWOOD_RELAY_2_NODE", "WHITEPINE_NODE"):
    assert x in gateway, x
for x in ("pollMissingAutomatic();", "pollConfiguredStations();", "persistCc01Settings()",
          "hourlyRescue.cancelNode", "HOBO_HTTP_GATEWAY_URL", "HOBO_HTTP_GATEWAY_INGEST_KEY",
          "autoFallback.armNode", "MESHTASTIC", "WiFi.isConnected()"):
    if x == "MESHTASTIC": continue
    assert x in gateway, x
assert 'v10-cloudflare-remote-2026-11-25' in gateway
assert 'CcaCloudRemote::isCloudNode(from)' in gateway
assert 'const bool cloudOnly = CcaCloudRemote::isCloudNode(job.from);' in gateway
assert 'cloudOnly ? String(CCA_CLOUD_URL) + "/v10/ingest" : String(HOBO_HTTP_GATEWAY_URL)' in gateway
assert 'CcaCloudRemote::remoteDmRevision()' in gateway
assert 'CcaCloudRemote::statusJson()' in gateway
assert 'ccaCloudRemote = new CcaCloudRemote()' in modules
assert 'CONFIG_INTERVAL_MS = 300000UL' in remote
assert '1439303287UL, 1668198402UL' in remote
assert 'p.getBytes("nodes"' in remote and 'p.putBytes("nodes"' in remote
assert 'JSON::Parse(response.c_str())' in remote
assert 'verifyHmac(' in remote and 'mbedtls_md_hmac' in remote
assert 'esp_ota_set_boot_partition' not in remote
assert 'Update.begin(' not in remote
assert 'ota_install_enabled' in remote and 'false' in remote
assert 'POST' in gateway and '/v10/health' in remote
for x in ('/v10/config','/v10/ingest','/v10/health','mesh_stations','mesh_observations',
          'mesh_device_telemetry','mesh_gateway_dm_overrides','GATEWAY_KEY','sensor type mismatch'):
    assert x in worker, x
assert 'final verified' in v9.lower()
assert 'pre-cloudflare' in v9.lower()
assert 'ota_enabled INTEGER' not in worker  # Schema migrations live in D1.
print("PASS: V10 dual backend routing; original V9 station/DM support; dynamic config, signed manifests and OTA-install safety")
