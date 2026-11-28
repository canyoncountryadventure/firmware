#pragma once

#include <cstddef>
#include <cstdint>

namespace CcaTlsMemory
{
constexpr uint32_t MAGIC = 0x544C5331;
constexpr size_t HISTORY = 6;
constexpr size_t OPERATIONS = 7;

struct Heap {
    uint32_t free;
    uint32_t largest;
    uint32_t minimum;
    uint32_t psram;
};

struct Record {
    uint32_t sequence;
    uint32_t boot;
    uint32_t uptime;
    uint32_t duration;
    uint32_t operation;
    uint32_t stage;
    int32_t http;
    int32_t tls;
    int32_t socketError;
    Heap points[4];
};

struct Log {
    uint32_t magic;
    uint32_t boot;
    uint32_t sequence;
    uint32_t active;
    uint32_t count;
    uint32_t next;
    uint32_t completed[OPERATIONS];
    uint32_t failed[OPERATIONS];
    Heap baseline;
    Record firstFailure;
    Record lastFailure;
    Record records[HISTORY];

    void beginBoot(uint32_t number, Heap heap);
    Record &start(uint32_t operation, uint32_t now, Heap heap);
    Record &current() { return records[(next + HISTORY - 1) % HISTORY]; }
    void finish(uint32_t now, Heap heap);
};
} // namespace CcaTlsMemory

#if defined(ARCH_ESP32) && defined(HELTEC_V4) && HAS_WIFI
#include <Arduino.h>
#include <WiFiClientSecure.h>

namespace CcaTlsMemory
{
void beginBoot(uint32_t number);
bool canAttempt(uint32_t operation);
String json(size_t history = HISTORY);
String currentHeapJson();

class Scope
{
  public:
    explicit Scope(uint32_t operation);
    ~Scope();
    void connecting();
    void connected(int result, int tls, int socketError);
    void response(int http, WiFiClientSecure &client);
    Scope(const Scope &) = delete;
    Scope &operator=(const Scope &) = delete;
};

class Client : public WiFiClientSecure
{
  public:
    explicit Client(Scope &scope) : trace(scope) {}
    using WiFiClientSecure::connect;
    int connect(const char *host, uint16_t port, int32_t timeout) override;

  private:
    Scope &trace;
};
} // namespace CcaTlsMemory
#endif
