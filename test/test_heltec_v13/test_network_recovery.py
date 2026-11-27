"""V13 repair integration: network recovery and full-reboot backstop."""
from pathlib import Path
root=Path(__file__).resolve().parents[2]
main=(root/"src/platform/esp32/main-esp32.cpp").read_text()
sup=(root/"src/modules/CcaGatewaySupervisor.h").read_text()
cloud=(root/"src/modules/CcaCloudRemote.cpp").read_text()
gw=(root/"src/modules/HoboHttpGatewayModule.cpp").read_text()
ota=(root/"src/modules/CcaRemoteOtaStage.cpp").read_text()
assert "APP_WATCHDOG_SECS 90" in main  # real stuck loop still reboots
assert "CcaGatewayRecovery::UploadStalled ||" in main
assert "action == CcaGatewayRecovery::WifiStalled" in main
assert main.index("action == CcaGatewayRecovery::WifiStalled") < main.index("} else if (action != CcaGatewayRecovery::None)")
assert "CcaNetworkRecovery::Action::Reboot" in main
assert "CcaNetworkRecovery::Action::Reconnect" in main
assert "rebootAtMsec = ccaNow + 5000UL" in main
assert "successfulConfigPolls()" in main
assert "requestHeltecWifiReconnect();" in main and "requestHeltecWifiReconnect();" in gw
assert "WiFi.disconnect(false);" not in main and "WiFi.reconnect();" not in main
wifi=(root/"src/mesh/wifi/WiFiAPClient.cpp").read_text()
assert "WiFi.disconnect(false, false);" in wifi
assert "void requestHeltecWifiReconnect()" in wifi
assert "recentlyHealthy(ccaNow, 5UL * 60UL * 1000UL)" in main
assert "lastAcceptedMs" in sup and "recentlyHealthy" in sup
assert "client.setHandshakeTimeout(12)" in cloud
assert "client.setHandshakeTimeout(8)" in cloud
assert "http.setConnectTimeout(8000)" in cloud
assert "client.setHandshakeTimeout(12)" in gw
assert "http.setConnectTimeout(12000)" in gw
for tag in ("CLOUD_CONFIG_TLS","CLOUD_HEALTH_TLS","SENSOR_UPLOAD_TLS"):
    assert tag in main
assert "CcaV13TlsOp operation(1)" in cloud and "CcaV13TlsOp operation(2)" in cloud
assert "CcaV13UploadOp operation;" in gw
assert "HELTEC-GATEWAY-V13" in cloud
assert "loaderReady()" in ota and "LOADER_RESERVED_BYTES = 0xA0000" in ota
assert "pollMissingAutomatic();" in gw and "pollConfiguredStations();" in gw
print("PASS: V13 bounded HTTPS, retained TLS trace, bounded network-triggered MCU reboot, OTA/DM retained")
