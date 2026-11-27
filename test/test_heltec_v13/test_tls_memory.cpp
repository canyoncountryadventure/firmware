#include "configuration.h"
#include "src/modules/CcaTlsMemory.h"
#include <cassert>
#include <iostream>
#include <type_traits>

uint32_t testNow = 0;
size_t testFree = 120000;
size_t testMinimum = testFree;
bool testConnectionFails = false;
unsigned testConnections = 0;
unsigned testDestructions = 0;

int main()
{
    using namespace CcaTlsMemory;
    static_assert(std::is_trivial<Log>::value, "RTC history must have no boot-time initializer");
    static_assert(sizeof(Log) < 1200, "Trace must stay bounded in RTC RAM");
    Log retained{};
    const Heap heap{120000, 90000, 80000, 2000000};
    retained.beginBoot(12, heap);
    for (uint32_t i = 0; i < 10000; ++i) {
        auto &record = retained.start(3, i * 100, heap);
        record.http = i % 5 ? 201 : -1;
        record.tls = record.http > 0 ? 0 : -16;
        retained.finish(i * 100 + 25, heap);
    }
    assert(retained.completed[3] == 10000 && retained.failed[3] == 2000);
    assert(retained.count == HISTORY && retained.firstFailure.sequence == 1);
    assert(retained.lastFailure.sequence == 9996);
    retained.start(1, UINT32_MAX - 10, heap);
    retained.beginBoot(13, heap);
    assert(retained.current().stage == 5 && retained.lastFailure.boot == 12);
    assert(retained.active == 0 && retained.firstFailure.sequence == 1);
    retained.start(1, UINT32_MAX - 10, heap).http = 200;
    retained.finish(15, heap);
    assert(retained.current().duration == 26);

    beginBoot(40);
    for (unsigned i = 0; i < 100; ++i) {
        testConnectionFails = i % 2;
        {
            Scope scope(i % 6 + 1);
            Client client(scope);
            WiFiClientSecure &httpClientReference = client;
            assert(httpClientReference.connect("example.invalid", 443, 8000) == !testConnectionFails);
            scope.response(testConnectionFails ? -1 : 200, client);
            ++testNow;
        }
        // The scope samples after the client's destructor has released its TLS
        // allocation.
        assert(testFree == 120000);
    }
    assert(testConnections == 100 && testDestructions == 100);
    beginBoot(41);
    const String report = json();
    assert(report.size() <= 4096);
    std::cout << report << '\n';
}
