from pathlib import Path


source = Path("src/modules/HoboHttpGatewayModule.cpp").read_text()

assert "constexpr uint32_t MILL_CREEK_NODE = 1683604927UL; // !6459c5bf" in source
assert "constexpr uint32_t SEED_RELAY_1_NODE = 647393146UL; // !26966f7a" in source
assert "HIDDEN_VALLEY_NODE = 1252758033UL; // !4aab9211" in source
assert "MINI_ME_NODE = 4241345683UL; // !fccdcc93" in source
assert "COTTONWOOD_RELAY_2_NODE = 1222051909UL; // !48d70845" in source
assert "from != SEED_RELAY_1_NODE && from != COTTONWOOD_RELAY_2_NODE" in source
assert "from != CC01_NODE && from != CC02_NODE && from != MILL_CREEK_NODE &&" in source
assert "from != SEED_RELAY_1_NODE" in source
assert "v8-missing-auto-fallback-2026-10-30" in source

print("PASS: V8 gateway allowlist identifies Mini Me and forwards Cottonwood Relay 2, Seed Relay 1 and Mill Creek")
