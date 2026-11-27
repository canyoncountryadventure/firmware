#include "configuration.h"
#include "CcaTlsMemory.h"

namespace CcaTlsMemory
{
void Log::beginBoot(uint32_t number, Heap heap)
{
    if (magic != MAGIC || next >= HISTORY || count > HISTORY) {
        *this = {};
        magic = MAGIC;
    }
    if (active) {
        current().stage = 5;
        lastFailure = current();
        if (!firstFailure.sequence)
            firstFailure = current();
        active = 0;
    }
    boot = number;
    baseline = heap;
}

Record &Log::start(uint32_t operation, uint32_t now, Heap heap)
{
    Record &record = records[next];
    record = {};
    record.sequence = ++sequence;
    record.boot = boot;
    record.uptime = now;
    record.operation = operation;
    record.stage = 1;
    record.points[0] = heap;
    next = (next + 1) % HISTORY;
    if (count < HISTORY)
        ++count;
    active = record.sequence;
    return record;
}

void Log::finish(uint32_t now, Heap heap)
{
    Record &record = current();
    record.duration = now - record.uptime;
    record.points[3] = heap;
    record.stage = 4;
    active = 0;
    if (record.operation < OPERATIONS) {
        ++completed[record.operation];
        if (record.http <= 0 || record.http >= 400)
            ++failed[record.operation];
    }
    if (record.http <= 0 || record.http >= 400) {
        lastFailure = record;
        if (!firstFailure.sequence)
            firstFailure = record;
    }
}
} // namespace CcaTlsMemory

#if defined(ARCH_ESP32) && defined(HELTEC_V4) && HAS_WIFI
#include <cerrno>
#include <esp_attr.h>
#include <esp_heap_caps.h>

namespace CcaTlsMemory
{
namespace
{
RTC_NOINIT_ATTR Log log;
constexpr uint32_t INTERNAL = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;

Heap sample()
{
    return {static_cast<uint32_t>(heap_caps_get_free_size(INTERNAL)),
            static_cast<uint32_t>(heap_caps_get_largest_free_block(INTERNAL)),
            static_cast<uint32_t>(heap_caps_get_minimum_free_size(INTERNAL)),
            static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM))};
}

void appendHeap(String &out, const Heap &heap)
{
    out += String(heap.free) + ',' + String(heap.largest) + ',' + String(heap.minimum) + ',' + String(heap.psram);
}

void appendRecord(String &out, const Record &record)
{
    if (!record.sequence) {
        out += "null";
        return;
    }
    out += '[' + String(record.sequence) + ',' + String(record.boot) + ',' + String(record.uptime) + ',' +
           String(record.duration) + ',' + String(record.operation) + ',' + String(record.stage) + ',' + String(record.http) +
           ',' + String(record.tls) + ',' + String(record.socketError);
    for (const Heap &heap : record.points) {
        out += ',';
        appendHeap(out, heap);
    }
    out += ']';
}
} // namespace

void beginBoot(uint32_t number)
{
    log.beginBoot(number, sample());
}

Scope::Scope(uint32_t operation)
{
    log.start(operation, millis(), sample());
}
Scope::~Scope()
{
    log.finish(millis(), sample());
}

void Scope::connecting()
{
    log.current().points[0] = sample();
}

void Scope::connected(int result, int tls, int socketError)
{
    Record &record = log.current();
    record.points[1] = sample();
    record.stage = 2;
    record.tls = result ? 0 : tls;
    record.socketError = result ? 0 : socketError;
}

void Scope::response(int http, WiFiClientSecure &client)
{
    Record &record = log.current();
    record.http = http;
    if (http <= 0) {
        char error[2] = {};
        record.tls = client.lastError(error, sizeof(error));
    }
    record.points[2] = sample();
    record.stage = 3;
}

int Client::connect(const char *host, uint16_t port, int32_t timeout)
{
    trace.connecting();
    errno = 0;
    const int result = WiFiClientSecure::connect(host, port, timeout);
    const int socketError = errno;
    char error[2] = {};
    trace.connected(result, result ? 0 : lastError(error, sizeof(error)), socketError);
    return result;
}

String currentHeapJson()
{
    const Heap heap = sample();
    return "{\"free\":" + String(heap.free) + ",\"largest\":" + String(heap.largest) + ",\"minimum\":" + String(heap.minimum) +
           ",\"psram\":" + String(heap.psram) + '}';
}

String json(size_t history)
{
    String out;
    out.reserve(2800);
    out = "{\"schema\":1,\"boot\":" + String(log.boot) + ",\"active\":" + String(log.active) + ",\"baseline\":[";
    appendHeap(out, log.baseline);
    out += "],\"completed\":[";
    for (size_t i = 0; i < OPERATIONS; ++i) {
        if (i)
            out += ',';
        out += String(log.completed[i]);
    }
    out += "],\"failed\":[";
    for (size_t i = 0; i < OPERATIONS; ++i) {
        if (i)
            out += ',';
        out += String(log.failed[i]);
    }
    out += "],\"first_failure\":";
    appendRecord(out, log.firstFailure);
    out += ",\"last_failure\":";
    appendRecord(out, log.lastFailure);
    out += ",\"records\":[";
    const size_t take = history < log.count ? history : log.count;
    for (size_t i = 0; i < take; ++i) {
        if (i)
            out += ',';
        appendRecord(out, log.records[(log.next + HISTORY - take + i) % HISTORY]);
    }
    out += "]}";
    return out;
}
} // namespace CcaTlsMemory
#endif
