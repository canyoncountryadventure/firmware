from pathlib import Path


source = Path("src/modules/HoboHttpGatewayModule.cpp").read_text()
fallback_policy = Path("src/modules/AutoFallbackPolicy.h").read_text()

assert "constexpr uint32_t MILL_CREEK_NODE = 1683604927UL; // !6459c5bf" in source
assert "constexpr uint32_t SEED_RELAY_1_NODE = 647393146UL; // !26966f7a" in source
assert "HIDDEN_VALLEY_NODE = 1252758033UL; // !4aab9211" in source
assert "MINI_ME_NODE = 4241345683UL; // !fccdcc93" in source
assert "COTTONWOOD_RELAY_2_NODE = 1222051909UL; // !48d70845" in source
assert "WHITEPINE_NODE = 3943979587UL; // !eb145a43" in source
assert "from != SEED_RELAY_1_NODE && from != COTTONWOOD_RELAY_2_NODE &&" in source
assert "from != WHITEPINE_NODE" in source
assert "3943979587UL" not in fallback_policy
assert "from != CC01_NODE && from != CC02_NODE && from != MILL_CREEK_NODE &&" in source
assert "from != SEED_RELAY_1_NODE" in source
assert "v10-cloudflare-remote-2026-11-25" in source or "v9-hour-boundary-2026-11-04" in source

print("PASS: V9 forwards Whitepine battery telemetry without adding sensor polling")
