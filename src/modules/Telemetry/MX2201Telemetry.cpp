#include "configuration.h"

#if defined(ARCH_NRF52) && defined(SEEED_XIAO_NRF52840_KIT)

#include "MX2201Telemetry.h"

#include "../mesh/generated/meshtastic/telemetry.pb.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "RTC.h"
#include "main.h"

#include <bluefruit.h>
#include <cstdint>
#include <cstring>

namespace
{

// ============================================================
// MX2201 identity
// ============================================================

// Logger BLE MAC:
// EB:9A:E4:52:6D:5F
//
// Nordic / Bluefruit byte order:
static const uint8_t HOBO_MAC[6] = {
    0x5F,
    0x6D,
    0x52,
    0xE4,
    0x9A,
    0xEB
};

// Service:
// 65e16e4f-ed4e-4641-ac49-83ccbce6cbcf
static const uint8_t HOBO_SERVICE_UUID[16] = {
    0xCF, 0xCB, 0xE6, 0xBC,
    0xCC, 0x83,
    0x49, 0xAC,
    0x41, 0x46,
    0x4E, 0xED,
    0x4F, 0x6E,
    0xE1, 0x65
};

// Command/notification characteristic:
// 65e16f4f-ed4e-4641-ac49-83ccbce6cbcf
static const uint8_t HOBO_CHAR_UUID[16] = {
    0xCF, 0xCB, 0xE6, 0xBC,
    0xCC, 0x83,
    0x49, 0xAC,
    0x41, 0x46,
    0x4E, 0xED,
    0x4F, 0x6F,
    0xE1, 0x65
};

BLEClientService hoboService(HOBO_SERVICE_UUID);
BLEClientCharacteristic hoboCharacteristic(HOBO_CHAR_UUID);

// ============================================================
// Proven MX2201 commands
// ============================================================

// Initialize logger:
// 01 01 04 05 1C 01 00
static const uint8_t CMD_INIT[] = {
    0x01,
    0x01,
    0x04,
    0x05,
    0x1C,
    0x01,
    0x00
};

// Read metadata block 0:
// 01 01 0A 0A 01 00 00 00 00 00 00 08 00
static const uint8_t CMD_READ0[] = {
    0x01,
    0x01,
    0x0A,
    0x0A,
    0x01,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x08,
    0x00
};

// Read metadata block 8:
// 01 01 0A 0A 01 00 00 08 00 00 00 08 00
static const uint8_t CMD_READ8[] = {
    0x01,
    0x01,
    0x0A,
    0x0A,
    0x01,
    0x00,
    0x00,
    0x08,
    0x00,
    0x00,
    0x00,
    0x08,
    0x00
};

// Status / write-pointer request.
// IMPORTANT: exactly 11 bytes.
//
// 01 01 08 04 05 00 00 00 00 00 00
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

// ============================================================
// Temperature decoder constants
// ============================================================

static constexpr uint16_t MIN_REASONABLE_RAW = 400;
static constexpr uint16_t MAX_REASONABLE_RAW = 2400;
static constexpr uint16_t MAX_RAW_STEP = 100;

// Cross-window sanity check.
// A real stream temperature cannot plausibly jump this far between
// consecutive accepted logger samples. This protects against a
// smooth-looking but incorrectly aligned 12-bit phase winning.
static constexpr uint16_t MAX_ACCEPTED_RAW_JUMP = 250;

static constexpr float RAW_TO_F_SLOPE =
    0.0771942720f;

static constexpr float RAW_TO_F_INTERCEPT =
    -52.2825573f;

static constexpr size_t MEMORY_READ_LENGTH = 64;
static constexpr size_t MEMORY_NIBBLE_COUNT =
    MEMORY_READ_LENGTH * 2;

static constexpr size_t MAX_DECODED_VALUES = 43;

// ============================================================
// Timing
// ============================================================

static constexpr uint32_t COMMAND_DELAY_MS = 400;
static constexpr uint32_t STATUS_TIMEOUT_MS = 3000;
static constexpr uint32_t MEMORY_TIMEOUT_MS = 4000;
static constexpr uint32_t SCAN_LENGTH_SECONDS = 5;
static constexpr uint32_t SCAN_RETRY_MS = 7000;

// Bench fallback if Meshtastic environmental telemetry
// interval is not configured.
static constexpr uint32_t DEFAULT_TX_INTERVAL_MS = 60000;

// ============================================================
// Protocol state
// ============================================================

enum class ProtocolState : uint8_t
{
    DISCONNECTED = 0,
    SEND_INIT,
    WAIT_INIT,
    SEND_META0,
    WAIT_META0,
    SEND_META8,
    WAIT_META8,
    SEND_STATUS,
    WAIT_STATUS,
    SEND_MEMORY,
    WAIT_MEMORY,
    RUNNING
};

bool hoboClientInitialized = false;
bool hoboConnected = false;

bool scanInProgress = false;
uint32_t scanStartedMs = 0;

ProtocolState protocolState =
    ProtocolState::DISCONNECTED;

uint32_t stateDueMs = 0;

// ============================================================
// Status values
// ============================================================

bool statusReady = false;

uint32_t currentWritePointer = 0;
uint32_t lastReadPointer = 0;

uint16_t loggerIntervalSeconds = 0;

// ============================================================
// Memory receive buffer
// ============================================================

uint8_t memoryBuffer[MEMORY_READ_LENGTH];

size_t memoryLength = 0;

bool memoryCollecting = false;
bool memoryReady = false;

// ============================================================
// Latest decoded temperature
// ============================================================

bool haveValidTemperature = false;

uint16_t latestRaw = 0;
float latestTemperatureC = 0.0f;
float latestTemperatureF = 0.0f;

uint16_t previousAcceptedRaw = 0;
bool havePreviousAcceptedRaw = false;

uint32_t lastTelemetrySentMs = 0;

// ============================================================
// Utility helpers
// ============================================================

bool timeReached(uint32_t now, uint32_t target)
{
    return static_cast<int32_t>(now - target) >= 0;
}

uint16_t rawDifference(
    uint16_t a,
    uint16_t b)
{
    return (a >= b) ? (a - b) : (b - a);
}

bool rawIsPlausible(uint16_t raw)
{
    return raw >= MIN_REASONABLE_RAW &&
           raw <= MAX_REASONABLE_RAW;
}

uint32_t getLoggerPollIntervalMs()
{
    uint32_t intervalMs;

    if (loggerIntervalSeconds == 0) {
        intervalMs = 10000;
    } else {
        intervalMs =
            static_cast<uint32_t>(loggerIntervalSeconds) *
            1000UL;
    }

    // Avoid hammering BLE if logger settings are strange.
    if (intervalMs < 1000) {
        intervalMs = 1000;
    }

    // Keep the MX2201 BLE protocol active even when the logger
    // measurement interval is long. The logger can still measure
    // every 60 seconds or longer; we only check its write pointer
    // every 10 seconds. Memory is read only when the pointer changes.
    if (intervalMs > 10000) {
        intervalMs = 10000;
    }

    return intervalMs;
}

uint32_t getTelemetryTransmitIntervalMs()
{
    uint32_t configuredSeconds =
        moduleConfig.telemetry.environment_update_interval;

    if (configuredSeconds == 0) {
        return DEFAULT_TX_INTERVAL_MS;
    }

    uint64_t intervalMs =
        static_cast<uint64_t>(configuredSeconds) *
        1000ULL;

    if (intervalMs > 0xFFFFFFFFULL) {
        intervalMs = 0xFFFFFFFFULL;
    }

    return static_cast<uint32_t>(intervalMs);
}

void setState(
    ProtocolState newState,
    uint32_t delayMs = 0)
{
    protocolState = newState;
    stateDueMs = millis() + delayMs;
}

// ============================================================
// BLE command writer
// ============================================================

bool writeHoboCommand(
    const uint8_t *command,
    uint16_t length,
    const char *description)
{
    if (!hoboConnected) {
        LOG_WARN(
            "MX2201: cannot send %s - not connected",
            description);

        return false;
    }

    uint16_t written =
        hoboCharacteristic.write(
            command,
            length);

    LOG_INFO(
        "MX2201 TX: %s, requested=%u written=%u",
        description,
        length,
        written);

    return written == length;
}

// ============================================================
// Build memory-read command
// ============================================================

void buildMemoryReadCommand(
    uint32_t address,
    uint8_t command[13])
{
    command[0] = 0x01;
    command[1] = 0x01;
    command[2] = 0x0A;
    command[3] = 0x0A;
    command[4] = 0x01;

    command[5] =
        static_cast<uint8_t>(
            (address >> 24) & 0xFF);

    command[6] =
        static_cast<uint8_t>(
            (address >> 16) & 0xFF);

    command[7] =
        static_cast<uint8_t>(
            (address >> 8) & 0xFF);

    command[8] =
        static_cast<uint8_t>(
            address & 0xFF);

    // 64-byte length, big endian.
    command[9] = 0x00;
    command[10] = 0x00;
    command[11] = 0x00;
    command[12] = 0x40;
}

// ============================================================
// 12-bit decoder
// ============================================================

struct PhaseResult
{
    bool valid;
    uint8_t phase;
    int32_t score;
    uint16_t stableCount;
    uint16_t latest;
    uint16_t recency;
};

PhaseResult evaluatePhase(
    const uint8_t *data,
    uint8_t phase)
{
    uint8_t nibbles[MEMORY_NIBBLE_COUNT];

    for (size_t i = 0;
         i < MEMORY_READ_LENGTH;
         ++i) {

        nibbles[i * 2] =
            static_cast<uint8_t>(
                (data[i] >> 4) & 0x0F);

        nibbles[i * 2 + 1] =
            static_cast<uint8_t>(
                data[i] & 0x0F);
    }

    uint16_t values[MAX_DECODED_VALUES];
    bool skipped[MAX_DECODED_VALUES];

    memset(values, 0, sizeof(values));
    memset(skipped, 0, sizeof(skipped));

    size_t valueCount = 0;

    for (size_t nibbleIndex = phase;
         nibbleIndex + 2 < MEMORY_NIBBLE_COUNT &&
         valueCount < MAX_DECODED_VALUES;
         nibbleIndex += 3) {

        uint16_t value =
            static_cast<uint16_t>(
                (static_cast<uint16_t>(
                     nibbles[nibbleIndex]) << 8) |
                (static_cast<uint16_t>(
                     nibbles[nibbleIndex + 1]) << 4) |
                static_cast<uint16_t>(
                    nibbles[nibbleIndex + 2]));

        values[valueCount++] = value;
    }

    // Optional control-record filtering.
    //
    // Correctly aligned MX2201 control records can decode as:
    // FFF E00 xxx
    //
    // They are ignored for scoring. They are NOT used to
    // determine the phase.
    for (size_t i = 0;
         i + 1 < valueCount;
         ++i) {

        if (values[i] == 0x0FFF &&
            values[i + 1] == 0x0E00) {

            skipped[i] = true;
            skipped[i + 1] = true;

            if (i + 2 < valueCount) {
                skipped[i + 2] = true;
            }
        }
    }

    PhaseResult result;

    result.valid = false;
    result.phase = phase;
    result.score = -1000000;
    result.stableCount = 0;
    result.latest = 0;
    result.recency = 0xFFFF;

    if (valueCount < 2) {
        return result;
    }

    // Start from the newest end of the memory window.
    // Locate the most recent plausible pair, then extend
    // that smooth run backwards.
    for (int end =
             static_cast<int>(valueCount) - 1;
         end >= 1;
         --end) {

        int previous = end - 1;

        if (skipped[end] ||
            skipped[previous]) {
            continue;
        }

        uint16_t currentRaw =
            values[end];

        uint16_t previousRaw =
            values[previous];

        if (!rawIsPlausible(currentRaw) ||
            !rawIsPlausible(previousRaw)) {
            continue;
        }

        if (rawDifference(
                currentRaw,
                previousRaw) >
            MAX_RAW_STEP) {
            continue;
        }

        uint16_t stableCount = 2;

        int cursor = previous;

        while (cursor - 1 >= 0) {

            int older = cursor - 1;

            if (skipped[cursor] ||
                skipped[older]) {
                break;
            }

            uint16_t newerRaw =
                values[cursor];

            uint16_t olderRaw =
                values[older];

            if (!rawIsPlausible(newerRaw) ||
                !rawIsPlausible(olderRaw)) {
                break;
            }

            if (rawDifference(
                    newerRaw,
                    olderRaw) >
                MAX_RAW_STEP) {
                break;
            }

            stableCount++;
            cursor--;
        }

        uint16_t recency =
            static_cast<uint16_t>(
                (valueCount - 1) -
                static_cast<size_t>(end));

        int32_t score =
            static_cast<int32_t>(
                stableCount) *
            100;

        // Prefer a smooth run ending near the newest data.
        score -=
            static_cast<int32_t>(
                recency) *
            7;

        // Continuity with the last accepted logger reading
        // helps reject an accidental smooth sequence.
        if (havePreviousAcceptedRaw &&
            rawDifference(
                currentRaw,
                previousAcceptedRaw) <=
                MAX_RAW_STEP) {

            score += 200;
        }

        result.valid = true;
        result.score = score;
        result.stableCount = stableCount;
        result.latest = currentRaw;
        result.recency = recency;

        // Because we are scanning newest -> oldest,
        // the first usable run is the most recent run.
        break;
    }

    return result;
}

bool decodeTemperature(
    const uint8_t *data,
    uint16_t &raw,
    float &temperatureF,
    float &temperatureC,
    uint8_t &selectedPhase,
    uint16_t &stableSamples)
{
    PhaseResult best;

    best.valid = false;
    best.phase = 0;
    best.score = -1000000;
    best.stableCount = 0;
    best.latest = 0;
    best.recency = 0xFFFF;

    for (uint8_t phase = 0;
         phase < 3;
         ++phase) {

        PhaseResult candidate =
            evaluatePhase(data, phase);

        if (candidate.valid) {
            LOG_INFO(
                "MX2201: phase %u score=%ld stable=%u latestRaw=%u recency=%u",
                candidate.phase,
                static_cast<long>(
                    candidate.score),
                candidate.stableCount,
                candidate.latest,
                candidate.recency);
        } else {
            LOG_INFO(
                "MX2201: phase %u no stable temperature sequence",
                phase);
        }

        if (candidate.valid &&
            havePreviousAcceptedRaw) {

            uint16_t acceptedJump =
                rawDifference(
                    candidate.latest,
                    previousAcceptedRaw);

            if (acceptedJump >
                MAX_ACCEPTED_RAW_JUMP) {

                // A large jump is suspicious if it is stale or
                // supported by only one short candidate sequence.
                //
                // However, a real rapid temperature change can
                // produce a new sequence at the newest end of the
                // logger memory. Once at least three consecutive
                // samples support that newest sequence, allow it.
                if (candidate.recency != 0 ||
                    candidate.stableCount < 3) {

                    LOG_WARN(
                        "MX2201: phase %u held by continuity, previousRaw=%u candidateRaw=%u jump=%u stable=%u recency=%u",
                        candidate.phase,
                        previousAcceptedRaw,
                        candidate.latest,
                        acceptedJump,
                        candidate.stableCount,
                        candidate.recency);

                    continue;
                }

                LOG_INFO(
                    "MX2201: phase %u confirmed large temperature change, previousRaw=%u candidateRaw=%u jump=%u stable=%u",
                    candidate.phase,
                    previousAcceptedRaw,
                    candidate.latest,
                    acceptedJump,
                    candidate.stableCount);
            }
        }

        // Recency is the primary discriminator. A long smooth
        // sequence deeper in the 64-byte window may be valid old
        // temperature history, but it must not override a newer
        // plausible sequence. Score breaks ties at equal recency.
        if (candidate.valid &&
            (!best.valid ||
             candidate.recency < best.recency ||
             (candidate.recency == best.recency &&
              candidate.score > best.score))) {

            best = candidate;
        }
    }

    if (!best.valid ||
        best.stableCount < 2) {

        LOG_WARN(
            "MX2201: temperature alignment confidence too low");

        return false;
    }

    raw = best.latest;
    selectedPhase = best.phase;
    stableSamples = best.stableCount;

    temperatureF =
        RAW_TO_F_SLOPE *
            static_cast<float>(raw) +
        RAW_TO_F_INTERCEPT;

    temperatureC =
        (temperatureF - 32.0f) *
        5.0f / 9.0f;

    return true;
}

// ============================================================
// BLE callbacks
// ============================================================

bool isTargetHobo(
    const ble_gap_evt_adv_report_t *report)
{
    if (report == nullptr) {
        return false;
    }

    return memcmp(
               report->peer_addr.addr,
               HOBO_MAC,
               sizeof(HOBO_MAC)) == 0;
}

void hoboNotifyCallback(
    BLEClientCharacteristic *characteristic,
    uint8_t *data,
    uint16_t len)
{
    (void)characteristic;

    if (data == nullptr ||
        len == 0) {
        return;
    }

    // --------------------------------------------------------
    // Status packet
    //
    // Example:
    // 01 02 04 05 00 04 05 32
    // 00 00 14 38
    // 00 0A ...
    //
    // bytes 8..11 = write pointer, big endian
    // bytes 12..13 = logging interval, big endian
    // --------------------------------------------------------

    if (len >= 14 &&
        data[0] == 0x01 &&
        data[1] == 0x02 &&
        data[2] == 0x04 &&
        data[3] == 0x05) {

        currentWritePointer =
            (static_cast<uint32_t>(
                 data[8]) << 24) |
            (static_cast<uint32_t>(
                 data[9]) << 16) |
            (static_cast<uint32_t>(
                 data[10]) << 8) |
            static_cast<uint32_t>(
                data[11]);

        loggerIntervalSeconds =
            static_cast<uint16_t>(
                (static_cast<uint16_t>(
                     data[12]) << 8) |
                static_cast<uint16_t>(
                    data[13]));

        statusReady = true;

        LOG_INFO(
            "MX2201 STATUS: pointer=0x%08lX interval=%u seconds",
            static_cast<unsigned long>(
                currentWritePointer),
            loggerIntervalSeconds);

        return;
    }

    // --------------------------------------------------------
    // Memory packets
    //
    // Working standalone reader showed a transport/framing
    // byte 0x0B at the beginning of each memory notification.
    // Strip that byte and concatenate the remaining payload.
    // --------------------------------------------------------

    if (memoryCollecting &&
        data[0] >= 0x01 &&
        data[0] <= 0x04) {

        uint8_t fragment = data[0];

        size_t payloadOffset = 0;
        size_t payloadLength = 0;

        switch (fragment) {

        case 0x01:
            // First fragment:
            // 1-byte fragment number + 4-byte response header
            // + 15 bytes of requested memory.
            payloadOffset = 5;

            if (len > payloadOffset) {
                payloadLength =
                    static_cast<size_t>(
                        len - payloadOffset);
            }

            if (payloadLength > 15) {
                payloadLength = 15;
            }

            break;

        case 0x02:
        case 0x03:
            // Middle fragments:
            // 1-byte fragment number + 19 memory bytes.
            payloadOffset = 1;

            if (len > payloadOffset) {
                payloadLength =
                    static_cast<size_t>(
                        len - payloadOffset);
            }

            if (payloadLength > 19) {
                payloadLength = 19;
            }

            break;

        case 0x04:
            // Final fragment:
            // 1-byte fragment number + final 11 memory bytes.
            payloadOffset = 1;

            if (len > payloadOffset) {
                payloadLength =
                    static_cast<size_t>(
                        len - payloadOffset);
            }

            if (payloadLength > 11) {
                payloadLength = 11;
            }

            break;

        default:
            return;
        }

        size_t available =
            MEMORY_READ_LENGTH -
            memoryLength;

        size_t copyLength =
            payloadLength;

        if (copyLength > available) {
            copyLength = available;
        }

        if (copyLength > 0) {

            memcpy(
                memoryBuffer + memoryLength,
                data + payloadOffset,
                copyLength);

            memoryLength += copyLength;
        }

        LOG_INFO(
            "MX2201 MEMORY: fragment=%u copied=%u collected=%u/64",
            fragment,
            static_cast<unsigned>(
                copyLength),
            static_cast<unsigned>(
                memoryLength));

        if (memoryLength >=
            MEMORY_READ_LENGTH) {

            memoryCollecting = false;
            memoryReady = true;

            LOG_INFO(
                "MX2201 MEMORY: complete 64-byte window received");
        }

        return;
    }

    // Metadata/init responses are intentionally not interpreted.
    // They are only required as part of the proven startup sequence.
    LOG_DEBUG(
        "MX2201 RX: %u bytes, first=0x%02X",
        len,
        data[0]);
}

void hoboDisconnectCallback(
    uint16_t connHandle,
    uint8_t reason)
{
    (void)connHandle;

    hoboConnected = false;

    memoryCollecting = false;
    memoryReady = false;
    statusReady = false;

    setState(
        ProtocolState::DISCONNECTED);

    LOG_WARN(
        "MX2201: disconnected, reason=0x%02X",
        reason);
}

void hoboConnectCallback(
    uint16_t connHandle)
{
    LOG_INFO(
        "MX2201: BLE connection established");

    LOG_INFO(
        "MX2201: discovering HOBO service");

    if (!hoboService.discover(
            connHandle)) {

        LOG_WARN(
            "MX2201: HOBO service not found");

        Bluefruit.disconnect(
            connHandle);

        return;
    }

    LOG_INFO(
        "MX2201: HOBO service found");

    LOG_INFO(
        "MX2201: discovering command characteristic");

    if (!hoboCharacteristic.discover()) {

        LOG_WARN(
            "MX2201: command characteristic not found");

        Bluefruit.disconnect(
            connHandle);

        return;
    }

    LOG_INFO(
        "MX2201: command characteristic found");

    if (!hoboCharacteristic.enableNotify()) {

        LOG_WARN(
            "MX2201: failed to enable notifications");

        Bluefruit.disconnect(
            connHandle);

        return;
    }

    hoboConnected = true;

    memoryLength = 0;
    memoryCollecting = false;
    memoryReady = false;
    statusReady = false;

    LOG_INFO(
        "========================================");

    LOG_INFO(
        "MX2201 CONNECTED AND READY");

    LOG_INFO(
        "Starting logger protocol");

    LOG_INFO(
        "========================================");

    setState(
        ProtocolState::SEND_INIT,
        250);
}

void hoboScanCallback(
    ble_gap_evt_adv_report_t *report)
{
    if (!isTargetHobo(report)) {

        // Bluefruit central scanner callbacks pause scanning
        // while the callback is being processed.
        Bluefruit.Scanner.resume();

        return;
    }

    LOG_INFO(
        "MX2201: target logger found");

    Bluefruit.Scanner.stop();

    scanInProgress = false;

    LOG_INFO(
        "MX2201: connecting");

    if (!Bluefruit.Central.connect(
            report)) {

        LOG_WARN(
            "MX2201: connection attempt failed");
    }
}

// ============================================================
// BLE client initialization
// ============================================================

void initializeHoboClient()
{
    LOG_INFO(
        "MX2201: initializing BLE central client");

    hoboService.begin();

    hoboCharacteristic.setNotifyCallback(
        hoboNotifyCallback);

    hoboCharacteristic.begin(
        &hoboService);

    Bluefruit.Central.setConnectCallback(
        hoboConnectCallback);

    Bluefruit.Central.setDisconnectCallback(
        hoboDisconnectCallback);

    Bluefruit.Scanner.setRxCallback(
        hoboScanCallback);

    Bluefruit.Scanner.restartOnDisconnect(
        false);

    // 100 ms scan interval, 50 ms scan window.
    // Bluefruit units are 0.625 ms.
    Bluefruit.Scanner.setInterval(
        160,
        80);

    Bluefruit.Scanner.useActiveScan(
        false);

    hoboClientInitialized = true;

    LOG_INFO(
        "MX2201: BLE central client initialized");
}

} // namespace

// ============================================================
// Module
// ============================================================

MX2201TelemetryModule::MX2201TelemetryModule()
    : concurrency::OSThread(
          "MX2201Telemetry"),
      ProtobufModule(
          "MX2201Telemetry",
          meshtastic_PortNum_TELEMETRY_APP,
          &meshtastic_Telemetry_msg)
{
    setIntervalFromNow(10000);
}

// ============================================================
// Received TELEMETRY_APP messages
// ============================================================

bool MX2201TelemetryModule::handleReceivedProtobuf(
    const meshtastic_MeshPacket &mp,
    meshtastic_Telemetry *decoded)
{
    (void)mp;
    (void)decoded;

    // This module publishes MX2201 measurements.
    // Existing Meshtastic telemetry handling can process
    // received environmental telemetry.
    return false;
}

// ============================================================
// Standard Meshtastic environmental telemetry
// ============================================================

bool MX2201TelemetryModule::sendTemperatureTelemetry(
    float temperatureC)
{
    meshtastic_Telemetry telemetry =
        meshtastic_Telemetry_init_zero;

    telemetry.time = getTime();

    telemetry.which_variant =
        meshtastic_Telemetry_environment_metrics_tag;

    telemetry.variant.environment_metrics =
        meshtastic_EnvironmentMetrics_init_zero;

    telemetry.variant.environment_metrics.has_temperature =
        true;

    telemetry.variant.environment_metrics.temperature =
        temperatureC;

    LOG_INFO(
        "========================================");

    LOG_INFO(
        "MX2201: SENDING STANDARD MESHTASTIC TELEMETRY");

    LOG_INFO(
        "MX2201: temperature = %.2f C / %.2f F",
        temperatureC,
        latestTemperatureF);

    LOG_INFO(
        "========================================");

    // Update the local node database immediately.
    nodeDB->updateTelemetry(
        nodeDB->getNodeNum(),
        telemetry,
        RX_SRC_LOCAL);

    // --------------------------------------------------------
    // Send over LoRa mesh
    // --------------------------------------------------------

    meshtastic_MeshPacket *meshPacket =
        allocDataProtobuf(telemetry);

    if (meshPacket == nullptr) {

        LOG_WARN(
            "MX2201: failed to allocate mesh telemetry packet");

        return false;
    }

    meshPacket->to =
        NODENUM_BROADCAST;

    meshPacket->decoded.want_response =
        false;

    if (config.device.role ==
        meshtastic_Config_DeviceConfig_Role_SENSOR) {

        meshPacket->priority =
            meshtastic_MeshPacket_Priority_RELIABLE;

    } else {

        meshPacket->priority =
            meshtastic_MeshPacket_Priority_BACKGROUND;
    }

    service->sendToMesh(
        meshPacket,
        RX_SRC_LOCAL,
        true);

    LOG_INFO(
        "MX2201: telemetry submitted to LoRa mesh");

    // --------------------------------------------------------
    // Also give the connected phone the same STANDARD
    // TELEMETRY_APP protobuf immediately for bench testing.
    // This does not create a custom packet type.
    // --------------------------------------------------------

    if (service->isToPhoneQueueEmpty()) {

        meshtastic_MeshPacket *phonePacket =
            allocDataProtobuf(telemetry);

        if (phonePacket != nullptr) {

            phonePacket->to =
                NODENUM_BROADCAST;

            phonePacket->decoded.want_response =
                false;

            phonePacket->priority =
                meshtastic_MeshPacket_Priority_BACKGROUND;

            service->sendToPhone(
                phonePacket);

            LOG_INFO(
                "MX2201: telemetry submitted to connected phone");
        }
    }

    return true;
}

// ============================================================
// Main non-blocking state machine
// ============================================================

int32_t MX2201TelemetryModule::runOnce()
{
    uint32_t now = millis();

    // --------------------------------------------------------
    // Wait until normal Meshtastic Bluetooth is running.
    // --------------------------------------------------------

    if (nrf52Bluetooth == nullptr) {

        LOG_INFO(
            "MX2201: waiting for Meshtastic Bluetooth startup");

        return 5000;
    }

    if (!config.bluetooth.enabled) {

        LOG_WARN(
            "MX2201: Meshtastic Bluetooth is disabled");

        return 30000;
    }

    // --------------------------------------------------------
    // Initialize central client exactly once.
    // --------------------------------------------------------

    if (!hoboClientInitialized) {
        initializeHoboClient();
    }

    // --------------------------------------------------------
    // If disconnected, scan for the logger.
    // --------------------------------------------------------

    if (!hoboConnected) {

        if (scanInProgress &&
            timeReached(
                now,
                scanStartedMs +
                    SCAN_RETRY_MS)) {

            scanInProgress = false;
        }

        if (!scanInProgress) {

            LOG_INFO(
                "MX2201: scanning for EB:9A:E4:52:6D:5F");

            bool started =
                Bluefruit.Scanner.start(
                    SCAN_LENGTH_SECONDS);

            if (started) {

                scanInProgress = true;
                scanStartedMs = now;

            } else {

                LOG_WARN(
                    "MX2201: could not start BLE scan");
            }
        }

        return 500;
    }

    // --------------------------------------------------------
    // Protocol state machine
    // --------------------------------------------------------

    switch (protocolState) {

    case ProtocolState::SEND_INIT:

        if (!timeReached(
                now,
                stateDueMs)) {
            break;
        }

        if (writeHoboCommand(
                CMD_INIT,
                sizeof(CMD_INIT),
                "INITIALIZE LOGGER")) {

            setState(
                ProtocolState::WAIT_INIT,
                COMMAND_DELAY_MS);

        } else {

            setState(
                ProtocolState::SEND_INIT,
                1000);
        }

        break;

    case ProtocolState::WAIT_INIT:

        if (timeReached(
                now,
                stateDueMs)) {

            setState(
                ProtocolState::SEND_META0);
        }

        break;

    case ProtocolState::SEND_META0:

        if (writeHoboCommand(
                CMD_READ0,
                sizeof(CMD_READ0),
                "READ METADATA BLOCK 0")) {

            setState(
                ProtocolState::WAIT_META0,
                COMMAND_DELAY_MS);

        } else {

            setState(
                ProtocolState::SEND_META0,
                1000);
        }

        break;

    case ProtocolState::WAIT_META0:

        if (timeReached(
                now,
                stateDueMs)) {

            setState(
                ProtocolState::SEND_META8);
        }

        break;

    case ProtocolState::SEND_META8:

        if (writeHoboCommand(
                CMD_READ8,
                sizeof(CMD_READ8),
                "READ METADATA BLOCK 8")) {

            setState(
                ProtocolState::WAIT_META8,
                COMMAND_DELAY_MS);

        } else {

            setState(
                ProtocolState::SEND_META8,
                1000);
        }

        break;

    case ProtocolState::WAIT_META8:

        if (timeReached(
                now,
                stateDueMs)) {

            setState(
                ProtocolState::SEND_STATUS);
        }

        break;

    case ProtocolState::SEND_STATUS:

        statusReady = false;

        if (writeHoboCommand(
                CMD_STATUS,
                sizeof(CMD_STATUS),
                "READ STATUS / WRITE POINTER")) {

            setState(
                ProtocolState::WAIT_STATUS,
                STATUS_TIMEOUT_MS);

        } else {

            setState(
                ProtocolState::SEND_STATUS,
                1000);
        }

        break;

    case ProtocolState::WAIT_STATUS:

        if (statusReady) {

            statusReady = false;

            if (currentWritePointer <
                MEMORY_READ_LENGTH) {

                LOG_WARN(
                    "MX2201: write pointer too small for 64-byte read");

                setState(
                    ProtocolState::RUNNING,
                    getLoggerPollIntervalMs());

                break;
            }

            // First read, or logger wrote a new sample.
            if (lastReadPointer == 0 ||
                currentWritePointer !=
                    lastReadPointer) {

                setState(
                    ProtocolState::SEND_MEMORY);

            } else {

                LOG_DEBUG(
                    "MX2201: pointer unchanged at 0x%08lX",
                    static_cast<unsigned long>(
                        currentWritePointer));

                setState(
                    ProtocolState::RUNNING,
                    getLoggerPollIntervalMs());
            }

        } else if (timeReached(
                       now,
                       stateDueMs)) {

            LOG_WARN(
                "MX2201: status response timeout");

            setState(
                ProtocolState::SEND_STATUS,
                1000);
        }

        break;

    case ProtocolState::SEND_MEMORY:
    {
        uint32_t readAddress =
            currentWritePointer -
            MEMORY_READ_LENGTH;

        uint8_t memoryCommand[13];

        buildMemoryReadCommand(
            readAddress,
            memoryCommand);

        memoryLength = 0;
        memoryReady = false;
        memoryCollecting = true;

        LOG_INFO(
            "MX2201: reading 64 bytes from 0x%08lX to pointer 0x%08lX",
            static_cast<unsigned long>(
                readAddress),
            static_cast<unsigned long>(
                currentWritePointer));

        if (writeHoboCommand(
                memoryCommand,
                sizeof(memoryCommand),
                "READ 64-BYTE MEMORY WINDOW")) {

            setState(
                ProtocolState::WAIT_MEMORY,
                MEMORY_TIMEOUT_MS);

        } else {

            memoryCollecting = false;

            setState(
                ProtocolState::SEND_STATUS,
                1000);
        }

        break;
    }

    case ProtocolState::WAIT_MEMORY:

        if (memoryReady) {

            memoryReady = false;
            memoryCollecting = false;

            uint16_t raw = 0;
            float temperatureF = 0.0f;
            float temperatureC = 0.0f;
            uint8_t selectedPhase = 0;
            uint16_t stableSamples = 0;

            bool decoded =
                decodeTemperature(
                    memoryBuffer,
                    raw,
                    temperatureF,
                    temperatureC,
                    selectedPhase,
                    stableSamples);

            // Do not repeatedly read the same window,
            // even if the first one does not yet have enough
            // alignment confidence.
            lastReadPointer =
                currentWritePointer;

            if (decoded) {

                latestRaw = raw;
                latestTemperatureF =
                    temperatureF;
                latestTemperatureC =
                    temperatureC;

                haveValidTemperature = true;

                previousAcceptedRaw =
                    raw;

                havePreviousAcceptedRaw =
                    true;

                LOG_INFO(
                    "========================================");

                LOG_INFO(
                    "MX2201 TEMPERATURE");

                LOG_INFO(
                    "Selected phase: %u",
                    selectedPhase);

                LOG_INFO(
                    "Stable recent samples: %u",
                    stableSamples);

                LOG_INFO(
                    "Raw: %u",
                    latestRaw);

                LOG_INFO(
                    "Water Temp: %.2f F",
                    latestTemperatureF);

                LOG_INFO(
                    "Water Temp: %.2f C",
                    latestTemperatureC);

                LOG_INFO(
                    "Logging interval: %u seconds",
                    loggerIntervalSeconds);

                LOG_INFO(
                    "========================================");

                // First valid measurement is transmitted
                // immediately.
                if (lastTelemetrySentMs == 0) {

                    if (sendTemperatureTelemetry(
                            latestTemperatureC)) {

                        lastTelemetrySentMs =
                            millis();
                    }
                }
            }

            setState(
                ProtocolState::RUNNING,
                getLoggerPollIntervalMs());

        } else if (timeReached(
                       now,
                       stateDueMs)) {

            LOG_WARN(
                "MX2201: memory read timeout, collected=%u/64",
                static_cast<unsigned>(
                    memoryLength));

            memoryCollecting = false;

            setState(
                ProtocolState::SEND_STATUS,
                1000);
        }

        break;

    case ProtocolState::RUNNING:

        if (timeReached(
                now,
                stateDueMs)) {

            setState(
                ProtocolState::SEND_STATUS);
        }

        break;

    case ProtocolState::DISCONNECTED:
    default:

        break;
    }

    // --------------------------------------------------------
    // Meshtastic transmission interval is independent of the
    // HOBO measurement/logging interval.
    // --------------------------------------------------------

    if (haveValidTemperature &&
        lastTelemetrySentMs != 0) {

        uint32_t telemetryIntervalMs =
            getTelemetryTransmitIntervalMs();

        if (static_cast<uint32_t>(
                now -
                lastTelemetrySentMs) >=
            telemetryIntervalMs) {

            if (sendTemperatureTelemetry(
                    latestTemperatureC)) {

                lastTelemetrySentMs =
                    millis();
            }
        }
    }

    return 100;
}

#endif