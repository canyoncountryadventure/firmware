from pathlib import Path


source = Path("src/modules/HoboHttpGatewayModule.cpp").read_text()

assert "constexpr uint32_t MILL_CREEK_NODE = 1683604927UL; // !6459c5bf" in source
assert "from != CC01_NODE && from != CC02_NODE && from != MILL_CREEK_NODE" in source
assert "v7-mill-creek-2026-10-26" in source

print("PASS: Mill Creek MX2001 and device packets are admitted by the V7 gateway allowlist")
