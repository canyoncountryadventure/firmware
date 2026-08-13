# MX2201 NEWREAD64 diagnostic patch
#
# Purpose:
#   Add a one-shot diagnostic command that asks the HOBO MX2201 for its
#   live sensor values using Onset's NEWREAD64 command. This test DOES NOT
#   replace the existing memory decoder or change production telemetry.
#
# Run from the Meshtastic firmware repository root:
#   powershell -ExecutionPolicy Bypass -File .\tools\apply-mx2201-newread-test.ps1

$ErrorActionPreference = 'Stop'

$path = 'src/modules/Telemetry/MX2201Telemetry.cpp'

if (-not (Test-Path $path)) {
    throw "Expected source file not found: $path. Run this script from the firmware repository root."
}

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$text = [System.IO.File]::ReadAllText((Resolve-Path $path))
$text = $text -replace "`r`n", "`n"

function Replace-Exact {
    param(
        [string]$Name,
        [string]$Old,
        [string]$New
    )

    if (-not $script:text.Contains($Old)) {
        throw "Patch stopped: expected block not found: $Name"
    }

    $script:text = $script:text.Replace($Old, $New)
}

# ---------------------------------------------------------------------------
# 1. Add Onset NEWREAD64 command immediately after the existing STATUS command.
# ---------------------------------------------------------------------------
$old = @'
static const uint8_t CMD_STATUS[] = {
    0x01,
    0x01,
    0x08,
    0x04,
    0x05,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00
};
'@

$new = @'
static const uint8_t CMD_STATUS[] = {
    0x01,
    0x01,
    0x08,
    0x04,
    0x05,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00
};

// Onset NEWREAD64 live-sensor request.
// Reverse engineered from HOBOconnect / OnsetSDK CommandNEWREAD64.
//
// 01 01 08 04 04 00 00 00 00 00 00
//
// This diagnostic command is intentionally separate from the proven
// historical-memory decoder. For this test we only log the response;
// existing Meshtastic telemetry behavior remains unchanged.
static const uint8_t CMD_NEWREAD64[] = {
    0x01,
    0x01,
    0x08,
    0x04,
    0x04,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00
};
'@
Replace-Exact 'CMD_NEWREAD64 command' $old $new

# ---------------------------------------------------------------------------
# 2. Add two diagnostic protocol states.
# ---------------------------------------------------------------------------
$old = @'
    SEND_META8,
    WAIT_META8,
    SEND_STATUS,
'@
$new = @'
    SEND_META8,
    WAIT_META8,
    SEND_NEWREAD,
    WAIT_NEWREAD,
    SEND_STATUS,
'@
Replace-Exact 'NEWREAD protocol states' $old $new

# ---------------------------------------------------------------------------
# 3. Add diagnostic capture state.
# ---------------------------------------------------------------------------
$old = @'
bool statusReady = false;

uint32_t currentWritePointer = 0;
'@
$new = @'
bool statusReady = false;

// Diagnostic-only capture flag for one NEWREAD64 response after startup.
bool newReadCaptureActive = false;

uint32_t currentWritePointer = 0;
'@
Replace-Exact 'NEWREAD capture flag' $old $new

# ---------------------------------------------------------------------------
# 4. Capture and decode the raw NEWREAD64 notification.
#    We search the notification for command/subcommand 04 04. The next
#    four bytes are the MX2201 TempSensor32 raw value in big-endian order.
# ---------------------------------------------------------------------------
$old = @'
    if (data == nullptr ||
        len == 0) {
        return;
    }

    // --------------------------------------------------------
    // Status packet
'@

$new = @'
    if (data == nullptr ||
        len == 0) {
        return;
    }

    // --------------------------------------------------------
    // Diagnostic Onset NEWREAD64 response capture.
    //
    // OnsetSDK processes the reassembled response as:
    //   04 04 [TempSensor32: 4 bytes big endian] [battery ...]
    //
    // Bluefruit exposes the raw transport notification, so locate the
    // adjacent 04 04 bytes rather than assuming a transport offset before
    // we have confirmed this exact MX2201 response on hardware.
    // --------------------------------------------------------

    if (newReadCaptureActive) {

        char hexLine[3 * 20 + 1];
        size_t pos = 0;

        for (uint16_t i = 0;
             i < len && i < 20;
             ++i) {

            int written = snprintf(
                hexLine + pos,
                sizeof(hexLine) - pos,
                "%02X%s",
                data[i],
                (i + 1 < len && i + 1 < 20) ? " " : "");

            if (written <= 0) {
                break;
            }

            pos += static_cast<size_t>(written);

            if (pos >= sizeof(hexLine)) {
                pos = sizeof(hexLine) - 1;
                break;
            }
        }

        hexLine[pos] = '\0';

        LOG_INFO(
            "MX2201 NEWREAD RAW: len=%u bytes=%s",
            len,
            hexLine);

        for (uint16_t i = 0;
             i + 5 < len;
             ++i) {

            if (data[i] == 0x04 &&
                data[i + 1] == 0x04) {

                uint32_t raw32 =
                    (static_cast<uint32_t>(data[i + 2]) << 24) |
                    (static_cast<uint32_t>(data[i + 3]) << 16) |
                    (static_cast<uint32_t>(data[i + 4]) << 8) |
                    static_cast<uint32_t>(data[i + 5]);

                float onsetTemperatureC =
                    static_cast<float>(raw32) *
                        175.72f / 4096.0f -
                    46.85f;

                float onsetTemperatureF =
                    onsetTemperatureC *
                        9.0f / 5.0f +
                    32.0f;

                LOG_INFO(
                    "========================================");
                LOG_INFO(
                    "MX2201 NEWREAD64 LIVE SENSOR");
                LOG_INFO(
                    "Raw32: %lu (0x%08lX)",
                    static_cast<unsigned long>(raw32),
                    static_cast<unsigned long>(raw32));
                LOG_INFO(
                    "Onset Temp: %.2f F / %.2f C",
                    onsetTemperatureF,
                    onsetTemperatureC);
                LOG_INFO(
                    "========================================");

                newReadCaptureActive = false;
                break;
            }
        }
    }

    // --------------------------------------------------------
    // Status packet
'@
Replace-Exact 'NEWREAD response capture' $old $new

# ---------------------------------------------------------------------------
# 5. Reset diagnostic capture on connect/disconnect.
# ---------------------------------------------------------------------------
$old = @'
    memoryCollecting = false;
    memoryReady = false;
    statusReady = false;

    setState(
'@
$new = @'
    memoryCollecting = false;
    memoryReady = false;
    statusReady = false;
    newReadCaptureActive = false;

    setState(
'@
Replace-Exact 'disconnect capture reset' $old $new

$old = @'
    memoryCollecting = false;
    memoryReady = false;
    statusReady = false;

    LOG_INFO(
        "========================================");
'@
$new = @'
    memoryCollecting = false;
    memoryReady = false;
    statusReady = false;
    newReadCaptureActive = false;

    LOG_INFO(
        "========================================");
'@
Replace-Exact 'connect capture reset' $old $new

# ---------------------------------------------------------------------------
# 6. Insert one NEWREAD64 request after metadata initialization, then continue
#    into the existing STATUS/memory path. Nothing else changes.
# ---------------------------------------------------------------------------
$old = @'
    case ProtocolState::WAIT_META8:

        if (timeReached(
                now,
                stateDueMs)) {

            setState(
                ProtocolState::SEND_STATUS);
        }

        break;

    case ProtocolState::SEND_STATUS:
'@

$new = @'
    case ProtocolState::WAIT_META8:

        if (timeReached(
                now,
                stateDueMs)) {

            setState(
                ProtocolState::SEND_NEWREAD);
        }

        break;

    case ProtocolState::SEND_NEWREAD:

        newReadCaptureActive = true;

        if (writeHoboCommand(
                CMD_NEWREAD64,
                sizeof(CMD_NEWREAD64),
                "READ LIVE SENSORS (NEWREAD64)")) {

            setState(
                ProtocolState::WAIT_NEWREAD,
                2000);

        } else {

            newReadCaptureActive = false;

            setState(
                ProtocolState::SEND_NEWREAD,
                1000);
        }

        break;

    case ProtocolState::WAIT_NEWREAD:

        if (timeReached(
                now,
                stateDueMs)) {

            newReadCaptureActive = false;

            setState(
                ProtocolState::SEND_STATUS);
        }

        break;

    case ProtocolState::SEND_STATUS:
'@
Replace-Exact 'NEWREAD startup state machine' $old $new

# Write UTF-8 without BOM, preserving the repo's CRLF-neutral content.
[System.IO.File]::WriteAllText(
    (Resolve-Path $path),
    ($text -replace "`n", "`r`n"),
    $utf8NoBom)

Write-Host ''
Write-Host 'NEWREAD64 diagnostic patch applied.' -ForegroundColor Green
Write-Host ''

# Verify whitespace/patch integrity.
& git diff --check
if ($LASTEXITCODE -ne 0) {
    throw 'git diff --check failed. Stop here and do not build.'
}

Write-Host 'git diff --check: clean' -ForegroundColor Green
Write-Host ''
Write-Host 'Changed files:'
& git status --short
Write-Host ''
Write-Host 'Next build with:'
Write-Host '& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e seeed_xiao_nrf52840_kit'
