from pathlib import Path
root=Path(__file__).resolve().parents[2]
gw=(root/"src/modules/HoboHttpGatewayModule.cpp").read_text()
p=(root/"src/modules/CcaCc01DmPoller.cpp").read_text()
assert "cca_gateway_reboot::isCommand(packet->decoded.payload.bytes" in gw
assert "isGatewayControlDm(&mp, nodeDB->getNodeNum())" in gw
assert "mp.public_key.size == 32" in p and "config.security.admin_key" in p
assert "nodeDB->isFavorite(sender)" in p
assert "cca_gateway_reboot::authorized(direct, mp.pki_encrypted" in p
assert p.index("cca_gateway_reboot::authorized") < p.index("rebootAtMsec = millis() + 20000UL")
assert "if (rebootAtMsec != 0)" in p
assert "REBOOT accepted; restarting in 20 seconds." in p
assert "WiFi" not in p[p.index("if (cca_gateway_reboot::isCommand(mp.decoded.payload.bytes, n))"):p.index("// Deliberately accept gateway control DMs")]
print("PASS: reboot DM routing, authorization, delayed reboot and offline independence")
