"""V7 source invariants for unified RAK distance firmware."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
rak = (root / "src/modules/Telemetry/HOBOMX2001MX2201MX2203/HOBOMX2001MX2201MX2203Telemetry.cpp").read_text()
water = (root / "src/modules/Telemetry/DistanceSensor/DistanceSensorModule.cpp").read_text()
variant = (root / "variants/nrf52840/rak4631/platformio.ini").read_text()
recovery = (root / "src/modules/Telemetry/HOBOSelfRecovery/HOBOSelfRecovery.cpp").read_text()

# HOBO radio work is opt-in/out per station and the saved OFF state gates
# startup, scanning, recovery and existing connections.
assert 'BLE_MODE_FILE_PATH[] = "/prefs/hobo_ble_mode.bin"' in rak
assert "HOBO BLE OFF" in rak and "HOBO BLE ON" in rak
assert "loadHoboBleMode()" in rak
assert "if (!hoboBleEnabled)\n        return 30000;" in rak
assert "Bluefruit.Scanner.stop()" in rak
assert "if (!hoboBleEnabled)\n        return false;" in rak
assert "HOBO BLE ON/OFF" in recovery

# One target must carry both runtime-selectable drivers. No default-sensor
# compiler symbol belongs to the V7 target.
assert "[env:rak4631_water_unified_v7]" in variant
v7_target = variant.split("[env:rak4631_water_unified_v7]", 1)[1]
assert "V5_DEFAULT_SENSOR_SEN0313" not in v7_target
assert "WATER SENSOR MB7388" in water
assert "WATER SENSOR SEN0313" in water
assert "DFRobotUARTDistanceDriver" in water and "MaxBotixMB7388DistanceDriver" in water

print("PASS: V7 unified sensor selection and persistent HOBO BLE central-off mode")
