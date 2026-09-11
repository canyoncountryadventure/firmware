# Canonical Field Self-Recovery Baseline

This branch is the canonical starting point for unattended nRF52 field nodes in this repository.

## Supported boards

- Seeed XIAO nRF52840 + Wio-SX1262
- RAK4631 on RAK19007 for new distance-sensor deployments

Existing RAK4631 Meshtastic board support is retained; project documentation should treat RAK19007 as the target base for the new distance-sensor work.

## Baseline guarantees

- 15-minute nRF52840 watchdog supervisor
- non-destructive remote `RECOVER` / `REBOOT`
- BLE disconnected-state recovery without tearing down a healthy link
- persistent field configuration
- battery telemetry support
- safer low-voltage margin for solar nodes
- Seeed battery-rise LPCOMP wake so a solar-recharged node can recover from SYSTEM OFF
- RAK existing LPCOMP wake retained with the safer low-voltage margin
- no recovery action erases NVS, node identity, NodeDB, channels, keys, or field calibration/state

## Source branches incorporated

- `hobo-self-recovery-seeed` — validated Seeed recovery + solar-wake line
- `hobo-self-recovery-rak4631` — validated RAK4631 recovery + solar-safe line

The common recovery modules are shared. Board-specific power/build handling remains under each board variant.

## New development branches

Distance-sensor development branches should start here:

- `distance-self-recovery-seeed`
- `distance-self-recovery-rak4631`

The distance subsystem will share one sensor-independent calibration/counting/stage layer and board/sensor adapters for:

- DFRobot SEN0590
- DFRobot SEN0311 / A02YYUW
- DFRobot SEN0313 / A01NYUB

Planned operating modes:

- `MODE WATER` — DM field calibration such as `CAL STAGE 1.42FT`, raw distance retained, stage derived from the installed reference.
- `MODE TRAIL` — DM `CAL CLEAR`, persistent baseline, hysteresis/fast rearm, daily and lifetime counts.

## Rule

Future unattended field-node firmware should inherit recovery/power protections from this branch rather than starting again from the older HOBO production branch.
