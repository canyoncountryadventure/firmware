from pathlib import Path
root=Path(__file__).resolve().parents[2]
gw=(root/"src/modules/HoboHttpGatewayModule.cpp").read_text()
p=(root/"src/modules/CcaCc01DmPoller.cpp").read_text()
assert "cca_gateway_reboot::isCommand(packet->decoded.payload.bytes" in gw
assert "isGatewayControlDm(&mp, nodeDB->getNodeNum())" in gw
start=p.index("if (cca_gateway_reboot::isCommand(mp.decoded.payload.bytes, n))")
end=p.index("// Deliberately accept gateway control DMs",start)
handler=p[start:end]
assert "mp.to == nodeDB->getNodeNum()" in handler
assert "cca_gateway_reboot::authorized(direct, mp.pki_encrypted, mp.channel)" in handler
assert "admin_key" not in handler and "isFavorite" not in handler
assert handler.index("cca_gateway_reboot::authorized") < handler.index("rebootAtMsec = millis() + 20000UL")
assert "if (rebootAtMsec != 0)" in handler
assert "REBOOT accepted; restarting in 20 seconds." in handler
assert "WiFi" not in handler
print("PASS: any-radio REBOOT DM routing, direct-message check, delayed reboot and offline independence")
