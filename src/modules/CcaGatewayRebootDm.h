#pragma once
#include <cstddef>
#include <cstdint>
namespace cca_gateway_reboot {
inline bool whitespace(uint8_t c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}
inline bool isCommand(const uint8_t *bytes, size_t n) {
    if (!bytes || n > 120) return false;
    size_t start = 0;
    while (start < n && whitespace(bytes[start])) ++start;
    while (n > start && whitespace(bytes[n-1])) --n;
    constexpr char expected[] = "REBOOT";
    if (n - start != sizeof(expected)-1) return false;
    for (size_t i=0; i<sizeof(expected)-1; ++i) {
        uint8_t c=bytes[start+i];
        if (c >= 'a' && c <= 'z') c -= 'a'-'A';
        if (c != static_cast<uint8_t>(expected[i])) return false;
    }
    return true;
}
inline bool authorized(bool direct, bool pki, bool hasAdmin, bool keyMatches, bool favorite) {
    return direct && pki && (hasAdmin ? keyMatches : favorite);
}
}
