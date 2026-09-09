#include "configuration.h"

#if defined(ARCH_NRF52) && defined(SEEED_XIAO_NRF52840_KIT)

#include "FuckingAroundWaterTelemetry.h"

#include "MeshService.h"
#include "NodeDB.h"
#include "main.h"

#include <Arduino.h>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{

static constexpr uint32_t WATER_SENSOR_BAUD = 9600;
static constexpr uint32_t WATER_SAMPLE_INTERVAL_MS = 60UL * 1000UL;
static constexpr uint32_t WATER_SAMPLE_WINDOW_MS = 1800UL;
static constexpr uint8_t WATER_READINGS_PER_SAMPLE = 9;
static constexpr uint8_t WATER_MIN_VALID_READINGS = 5;
static constexpr uint8_t WATER_FAULT_FAILURE_LIMIT = 3;
static constexpr float WATER_FULL_DISTANCE_MM = 224.0f;
static constexpr float WATER_EMPTY_DISTANCE_MM = 406.4f;
static constexpr float WATER_REFILL_DELTA_PERCENT = 10.0f;

bool reachedWaterTime(uint32_t now, uint32_t target)
{
    return static_cast<int32_t>(now - target) >= 0;
}

void normalizeWaterCommand(const uint8_t *bytes, size_t size, char *out, size_t outSize)
{
    if (out == nullptr || outSize == 0)
        return;

    out[0] = '\0';
    if (bytes == nullptr || size == 0)
        return;

    size_t n = size;
    if (n > outSize - 1)
        n = outSize - 1;
    memcpy(out, bytes, n);
    out[n] = '\0';

    char *start = out;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')
        ++start;
    if (*start == '/')
        ++start;

    if (start != out)
        memmove(out, start, strlen(start) + 1);

    size_t len = strlen(out);
    while (len > 0 &&
           (out[len - 1] == ' ' || out[len - 1] == '\t' ||
            out[len - 1] == '\r' || out[len - 1] == '\n')) {
        out[--len] = '\0';
    }

    for (size_t i = 0; out[i] != '\0'; ++i)
        out[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[i])));
}

void sortWaterReadings(uint16_t *values, uint8_t count)
{
    for (uint8_t i = 1; i < count; ++i) {
        uint16_t key = values[i];
        int j = static_cast<int>(i) - 1;
        while (j >= 0 && values[j] > key) {
            values[j + 1] = values[j];
            --j;
        }
        values[j + 1] = key;
    }
}

bool readWaterFrame(uint16_t &distanceMm)
{
    while (Serial1.available()) {
        const uint8_t first = static_cast<uint8_t>(Serial1.read());
        if (first != 0xFF)
            continue;

        const uint32_t frameStart = millis();
        uint8_t frame[4] = {0xFF, 0, 0, 0};
        uint8_t index = 1;

        while (index < 4 && millis() - frameStart < 40UL) {
            if (Serial1.available())
                frame[index++] = static_cast<uint8_t>(Serial1.read());
            else
                delay(1);
        }

        if (index != 4)
            return false;

        const uint8_t checksum =
            static_cast<uint8_t>((frame[0] + frame[1] + frame[2]) & 0xFF);
        if (checksum != frame[3])
            return false;

        distanceMm =
            static_cast<uint16_t>((static_cast<uint16_t>(frame[1]) << 8) | frame[2]);
        return true;
    }

    return false;
}

} // namespace

FuckingAroundWaterTelemetryModule::FuckingAroundWaterTelemetryModule()
    : HOBOMX2001MX2201MX2203TelemetryModule()
{
}

void FuckingAroundWaterTelemetryModule::initializeWaterSensor()
{
    if (waterInitialized)
        return;

    // XIAO nRF52840 hardware UART defaults to D7 RX / D6 TX.
    // A02YYUW TX -> D7. The sensor RX/blue wire remains floating.
    Serial1.begin(WATER_SENSOR_BAUD);

    while (Serial1.available())
        Serial1.read();

    waterInitialized = true;
    nextWaterSampleMs = millis() + 1000UL;
    LOG_INFO("Fucking Around water: A02YYUW UART started at 9600 baud, 60-second interval");
}

bool FuckingAroundWaterTelemetryModule::collectMedianDistance(
    uint16_t &medianMm,
    uint8_t &validCount)
{
    uint16_t readings[WATER_READINGS_PER_SAMPLE] = {};
    validCount = 0;

    // Discard stale UART bytes so the sample represents the current surface.
    while (Serial1.available())
        Serial1.read();

    const uint32_t start = millis();
    while (validCount < WATER_READINGS_PER_SAMPLE &&
           millis() - start < WATER_SAMPLE_WINDOW_MS) {
        uint16_t mm = 0;
        if (readWaterFrame(mm)) {
            // Keep only physically plausible A02YYUW readings for this setup.
            if (mm >= 30 && mm <= 4500)
                readings[validCount++] = mm;
        } else {
            delay(2);
        }
    }

    if (validCount < WATER_MIN_VALID_READINGS)
        return false;

    sortWaterReadings(readings, validCount);
    medianMm = readings[validCount / 2];
    return true;
}

float FuckingAroundWaterTelemetryModule::distanceToPercent(uint16_t distanceMm) const
{
    float percent =
        100.0f * (WATER_EMPTY_DISTANCE_MM - static_cast<float>(distanceMm)) /
        (WATER_EMPTY_DISTANCE_MM - WATER_FULL_DISTANCE_MM);

    if (percent > 100.0f)
        percent = 100.0f;
    if (percent < 0.0f)
        percent = 0.0f;
    return percent;
}

bool FuckingAroundWaterTelemetryModule::updateWaterAlertFilter(
    uint16_t distanceMm,
    uint16_t &filteredDistanceMm,
    float &filteredPercent)
{
    waterAlertDistanceHistory[waterAlertHistoryIndex] = distanceMm;
    waterAlertHistoryIndex =
        static_cast<uint8_t>((waterAlertHistoryIndex + 1) % WATER_ALERT_HISTORY_SIZE);

    if (waterAlertHistoryCount < WATER_ALERT_HISTORY_SIZE)
        ++waterAlertHistoryCount;

    // Do not arm water-level threshold/refill alerts until five complete
    // minute-level samples exist. Each minute sample is already a median of
    // nine UART readings, so this confirmation window represents up to 45
    // raw sensor frames.
    if (waterAlertHistoryCount < WATER_ALERT_HISTORY_SIZE)
        return false;

    uint16_t sorted[WATER_ALERT_HISTORY_SIZE] = {};
    for (uint8_t i = 0; i < WATER_ALERT_HISTORY_SIZE; ++i)
        sorted[i] = waterAlertDistanceHistory[i];

    sortWaterReadings(sorted, WATER_ALERT_HISTORY_SIZE);

    // Five-sample trimmed mean: discard the single lowest and highest
    // minute-level distances, then average the middle three. One isolated
    // outlier therefore has zero influence on the alert decision.
    const uint32_t middleSum =
        static_cast<uint32_t>(sorted[1]) +
        static_cast<uint32_t>(sorted[2]) +
        static_cast<uint32_t>(sorted[3]);

    filteredDistanceMm = static_cast<uint16_t>((middleSum + 1U) / 3U);
    filteredPercent = distanceToPercent(filteredDistanceMm);
    return true;
}

void FuckingAroundWaterTelemetryModule::resetWaterAlertLadder(float percent)
{
    if (percent > 90.0f) nextWaterAlertThreshold = 90;
    else if (percent > 80.0f) nextWaterAlertThreshold = 80;
    else if (percent > 70.0f) nextWaterAlertThreshold = 70;
    else if (percent > 60.0f) nextWaterAlertThreshold = 60;
    else if (percent > 50.0f) nextWaterAlertThreshold = 50;
    else if (percent > 40.0f) nextWaterAlertThreshold = 40;
    else if (percent > 30.0f) nextWaterAlertThreshold = 30;
    else if (percent > 20.0f) nextWaterAlertThreshold = 20;
    else if (percent > 10.0f) nextWaterAlertThreshold = 10;
    else if (percent > 0.0f) nextWaterAlertThreshold = 0;
    else nextWaterAlertThreshold = -10;
}

bool FuckingAroundWaterTelemetryModule::sendWaterText(
    uint32_t destination,
    uint8_t channel,
    const char *text,
    bool ack)
{
    if (text == nullptr)
        return false;

    meshtastic_MeshPacket *packet = allocDataPacket();
    if (packet == nullptr) {
        LOG_WARN("Fucking Around water: text packet allocation failed");
        return false;
    }

    size_t len = strlen(text);
    if (len > sizeof(packet->decoded.payload.bytes))
        len = sizeof(packet->decoded.payload.bytes);

    memcpy(packet->decoded.payload.bytes, text, len);
    packet->decoded.payload.size = len;
    packet->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    packet->decoded.want_response = false;
    packet->to = destination;
    packet->channel = channel;
    packet->want_ack = ack;
    packet->priority = meshtastic_MeshPacket_Priority_RELIABLE;

    service->sendToMesh(packet, RX_SRC_LOCAL, true);
    return true;
}

void FuckingAroundWaterTelemetryModule::sendWaterEvent(
    const char *type,
    int threshold,
    float eventPercent,
    uint16_t eventDistanceMm)
{
    char text[180] = {};

    const float percent = eventPercent >= 0.0f ? eventPercent : currentWaterPercent;
    const uint16_t distanceMm = eventDistanceMm > 0 ? eventDistanceMm : currentWaterDistanceMm;
    const float inches = distanceMm / 25.4f;

    if (threshold >= 0) {
        snprintf(
            text,
            sizeof(text),
            "WATER_ALERT|%s|threshold=%d|pct=%.1f|mm=%u|in=%.2f",
            type,
            threshold,
            percent,
            distanceMm,
            inches);
    } else {
        snprintf(
            text,
            sizeof(text),
            "WATER_ALERT|%s|pct=%.1f|mm=%u|in=%.2f",
            type,
            percent,
            distanceMm,
            inches);
    }

    // Channel 0 broadcast is visible to an ordinary Meshtastic gateway listener.
    sendWaterText(NODENUM_BROADCAST, 0, text, false);
}

void FuckingAroundWaterTelemetryModule::processWaterAlerts(
    float percent,
    uint16_t distanceMm)
{
    (void)percent;

    uint16_t filteredDistanceMm = 0;
    float filteredPercent = 0.0f;
    if (!updateWaterAlertFilter(distanceMm, filteredDistanceMm, filteredPercent)) {
        LOG_INFO(
            "Fucking Around water: alert filter warming %u/%u",
            waterAlertHistoryCount,
            WATER_ALERT_HISTORY_SIZE);
        return;
    }

    confirmedWaterDistanceMm = filteredDistanceMm;
    confirmedWaterPercent = filteredPercent;

    if (!haveConfirmedWaterReading) {
        haveConfirmedWaterReading = true;
        previousWaterPercent = confirmedWaterPercent;
        resetWaterAlertLadder(confirmedWaterPercent);
        LOG_INFO(
            "Fucking Around water: alert filter armed %.1f%% %u mm",
            confirmedWaterPercent,
            confirmedWaterDistanceMm);
        return;
    }

    if (previousWaterPercent >= 0.0f &&
        confirmedWaterPercent >= previousWaterPercent + WATER_REFILL_DELTA_PERCENT) {
        resetWaterAlertLadder(confirmedWaterPercent);
        sendWaterEvent(
            "refill",
            -1,
            confirmedWaterPercent,
            confirmedWaterDistanceMm);
    }

    while (nextWaterAlertThreshold >= 0 &&
           confirmedWaterPercent <= nextWaterAlertThreshold) {
        const int crossed = nextWaterAlertThreshold;
        sendWaterEvent(
            "threshold",
            crossed,
            confirmedWaterPercent,
            confirmedWaterDistanceMm);
        nextWaterAlertThreshold -= 10;
    }

    previousWaterPercent = confirmedWaterPercent;

    LOG_INFO(
        "Fucking Around water: confirmed alert value %.1f%% %u mm (5-sample trimmed mean)",
        confirmedWaterPercent,
        confirmedWaterDistanceMm);
}

bool FuckingAroundWaterTelemetryModule::sampleWater(bool processAlerts)
{
    uint16_t medianMm = 0;
    uint8_t validCount = 0;

    if (!collectMedianDistance(medianMm, validCount)) {
        currentWaterValidCount = validCount;
        if (consecutiveWaterFailures < 255)
            ++consecutiveWaterFailures;

        if (consecutiveWaterFailures >= WATER_FAULT_FAILURE_LIMIT && !waterFaultActive) {
            waterFaultActive = true;
            sendWaterEvent("sensor_fault");
        }

        LOG_WARN(
            "Fucking Around water: sample failed valid=%u consecutive=%u",
            validCount,
            consecutiveWaterFailures);
        return false;
    }

    const bool recovering = waterFaultActive;
    waterFaultActive = false;
    consecutiveWaterFailures = 0;
    currentWaterDistanceMm = medianMm;
    currentWaterValidCount = validCount;
    currentWaterPercent = distanceToPercent(medianMm);
    lastWaterSampleMs = millis();

    if (!haveWaterReading)
        haveWaterReading = true;

    if (processAlerts)
        processWaterAlerts(currentWaterPercent, currentWaterDistanceMm);

    LOG_INFO(
        "Fucking Around water: %.1f%% %u mm %.2f in median=%u",
        currentWaterPercent,
        currentWaterDistanceMm,
        currentWaterDistanceMm / 25.4f,
        currentWaterValidCount);

    if (recovering)
        sendWaterEvent("sensor_recovered");

    return true;
}

void FuckingAroundWaterTelemetryModule::sendWaterStatus(
    uint32_t destination,
    uint8_t channel,
    bool raw)
{
    char reply[220] = {};

    if (!haveWaterReading) {
        snprintf(
            reply,
            sizeof(reply),
            "WATER unavailable | no valid A02YYUW sample yet | failures=%u",
            consecutiveWaterFailures);
        sendWaterText(destination, channel, reply, true);
        return;
    }

    const uint32_t ageSeconds = (millis() - lastWaterSampleMs) / 1000UL;

    if (raw) {
        if (haveConfirmedWaterReading) {
            snprintf(
                reply,
                sizeof(reply),
                "WATER RAW | median=%u mm | %.2f in | valid=%u/9 | alert=%.1f%%/%u mm | window=5/5 | failures=%u | age=%lus",
                currentWaterDistanceMm,
                currentWaterDistanceMm / 25.4f,
                currentWaterValidCount,
                confirmedWaterPercent,
                confirmedWaterDistanceMm,
                consecutiveWaterFailures,
                static_cast<unsigned long>(ageSeconds));
        } else {
            snprintf(
                reply,
                sizeof(reply),
                "WATER RAW | median=%u mm | %.2f in | valid=%u/9 | alert window=%u/5 warming | failures=%u | age=%lus",
                currentWaterDistanceMm,
                currentWaterDistanceMm / 25.4f,
                currentWaterValidCount,
                waterAlertHistoryCount,
                consecutiveWaterFailures,
                static_cast<unsigned long>(ageSeconds));
        }
    } else {
        char nextThreshold[16] = {};
        if (nextWaterAlertThreshold >= 0)
            snprintf(nextThreshold, sizeof(nextThreshold), "%d%%", nextWaterAlertThreshold);
        else
            snprintf(nextThreshold, sizeof(nextThreshold), "none");

        if (haveConfirmedWaterReading) {
            snprintf(
                reply,
                sizeof(reply),
                "WATER %.1f%% | %.2f in | %u mm | alert-filter %.1f%% | %s | next %s | age %lus",
                currentWaterPercent,
                currentWaterDistanceMm / 25.4f,
                currentWaterDistanceMm,
                confirmedWaterPercent,
                waterFaultActive ? "FAULT" : "OK",
                nextThreshold,
                static_cast<unsigned long>(ageSeconds));
        } else {
            snprintf(
                reply,
                sizeof(reply),
                "WATER %.1f%% | %.2f in | %u mm | alert-filter warming %u/5 | %s | age %lus",
                currentWaterPercent,
                currentWaterDistanceMm / 25.4f,
                currentWaterDistanceMm,
                waterAlertHistoryCount,
                waterFaultActive ? "FAULT" : "OK",
                static_cast<unsigned long>(ageSeconds));
        }
    }

    sendWaterText(destination, channel, reply, true);
}

ProcessMessage FuckingAroundWaterTelemetryModule::handleReceived(
    const meshtastic_MeshPacket &mp)
{
    if (mp.decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP && nodeDB != nullptr) {
        const uint32_t ourNode = nodeDB->getNodeNum();
        if (mp.to == ourNode && mp.from != ourNode) {
            char command[40] = {};
            normalizeWaterCommand(
                mp.decoded.payload.bytes,
                mp.decoded.payload.size,
                command,
                sizeof(command));

            if (strcmp(command, "WATER") == 0 || strcmp(command, "WATER STATUS") == 0) {
                sendWaterStatus(mp.from, mp.channel, false);
                return ProcessMessage::CONTINUE;
            }

            if (strcmp(command, "WATER RAW") == 0) {
                sendWaterStatus(mp.from, mp.channel, true);
                return ProcessMessage::CONTINUE;
            }

            if (strcmp(command, "READ WATER") == 0 || strcmp(command, "WATER NOW") == 0) {
                const bool ok = sampleWater(true);
                if (ok)
                    sendWaterStatus(mp.from, mp.channel, false);
                else
                    sendWaterText(mp.from, mp.channel, "WATER read failed: not enough valid A02YYUW frames", true);
                return ProcessMessage::CONTINUE;
            }

            if (strcmp(command, "WATER HELP") == 0) {
                sendWaterText(
                    mp.from,
                    mp.channel,
                    "WATER commands: WATER | READ WATER | WATER STATUS | WATER RAW | WATER NOW | WATER HELP | sample=60s | alerts=5-sample trimmed mean",
                    true);
                return ProcessMessage::CONTINUE;
            }
        }
    }

    // Preserve every existing HOBO command and telemetry behavior, including READ.
    return HOBOMX2001MX2201MX2203TelemetryModule::handleReceived(mp);
}

int32_t FuckingAroundWaterTelemetryModule::runOnce()
{
    initializeWaterSensor();

    const uint32_t now = millis();
    if (reachedWaterTime(now, nextWaterSampleMs)) {
        sampleWater(true);
        nextWaterSampleMs = millis() + WATER_SAMPLE_INTERVAL_MS;
    }

    return HOBOMX2001MX2201MX2203TelemetryModule::runOnce();
}

#endif
