# Fucking Around — A02YYUW Cat Bowl Water Monitor

Experimental branch: `fucking-around`

## Hardware

- Seeed XIAO nRF52840 + Wio-SX1262
- DFRobot A02YYUW / SEN0311
- Sensor TX -> XIAO D7 / RX
- Sensor VCC -> 3V3
- Sensor GND -> GND
- Sensor RX / blue control wire -> floating

## Calibration

- 100% full: 224 mm / 8.82 in sensor-to-water
- 0% full: 406.4 mm / 16.00 in sensor-to-bottom
- Percent full is linear between those two calibration points.

## Sampling

- Sample interval: 60 seconds
- Each sample: median of 9 UART readings
- Minimum accepted frames: 5
- Sensor fault alert after 3 consecutive failed samples
- Sensor recovered alert on first subsequent valid sample

## Meshtastic commands

Direct-message the node with any of the following. Commands are case-insensitive and may optionally start with `/`.

- `WATER` — latest percent, distance, health, next alert threshold, sample age
- `WATER STATUS` — same as `WATER`
- `READ WATER` — force a fresh 9-reading median sample and return it
- `WATER NOW` — alias for `READ WATER`
- `WATER RAW` — latest median distance, valid-frame count, failure count, sample age
- `WATER HELP` — command list

The existing HOBO commands remain available and unchanged, including `READ`, `LOGGER`, `LOCK`, and `UNLOCK`.

Example response:

`WATER 73.4% | 10.73 in | 273 mm | OK | next 70% | age 18s`

## Alert behavior

The node broadcasts a short `WATER_ALERT|...` Meshtastic text message when:

- level crosses 90%, 80%, 70%, 60%, 50%, 40%, 30%, 20%, 10%, or 0%
- level rises at least 10 percentage points between valid samples (refill)
- sensor fails 3 consecutive minute samples
- sensor recovers after a fault

A refill resets the downward threshold ladder.

## Email without Neon

`tools/water_github_email_gateway.py` listens to an Internet-connected Meshtastic gateway radio. It ignores ordinary messages and reacts only to `WATER_ALERT|...` messages.

For each water alert it creates a GitHub issue in `canyoncountryadventure/firmware` and assigns `canyoncountryadventure`. GitHub's normal issue-notification system then provides the email notification when issue email notifications are enabled for the account/repository.

The gateway performs no Neon or other database writes.

### Gateway requirements

Python packages:

`py -m pip install meshtastic pypubsub`

Set a fine-grained GitHub token in the process environment with Issues read/write permission for this repository. Do not store the token in the repository or script.

PowerShell for the current window only:

`$env:GITHUB_TOKEN="YOUR_TOKEN"`

Run the gateway, substituting the radio COM port:

`py tools/water_github_email_gateway.py --port COM11`

Closing that PowerShell window removes the process environment variable. The gateway must be running on an Internet-connected computer/radio bridge for GitHub/email alerts to leave the mesh.

## Data storage

This experimental water path intentionally has no Neon/database upload. Readings stay on the node except for Meshtastic command replies and alert messages.
