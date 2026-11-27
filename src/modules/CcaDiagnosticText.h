#pragma once

#include <cstddef>
#include <cstdio>
#include <cstring>

namespace cca_diagnostic_text {
// Leave room for protobuf fields, the radio header and PKI encryption.
constexpr size_t MAX_REPLY_BYTES = 200;
constexpr size_t PAGE_BYTES = 170;

inline bool formatPage(const char *text, const char *argument, char *out,
                       size_t capacity) {
  if (!text || !argument || !out || capacity < MAX_REPLY_BYTES + 1)
    return false;
  const size_t length = std::strlen(text);
  const size_t pages = length ? (length + PAGE_BYTES - 1) / PAGE_BYTES : 1;
  size_t page = 1;
  bool valid = true;
  if (*argument) {
    page = 0;
    for (const char *p = argument; *p; ++p) {
      if (*p < '0' || *p > '9' || page > pages) {
        valid = false;
        break;
      }
      page = page * 10 + static_cast<size_t>(*p - '0');
    }
  }
  if (!valid || page == 0 || page > pages) {
    std::snprintf(out, capacity, "Use DIAG or DIAG <page>; pages 1-%u",
                  static_cast<unsigned>(pages));
    return false;
  }
  std::snprintf(out, capacity, "DIAG %u/%u\n%.*s", static_cast<unsigned>(page),
                static_cast<unsigned>(pages), static_cast<int>(PAGE_BYTES),
                text + (page - 1) * PAGE_BYTES);
  if (page < pages) {
    const size_t used = std::strlen(out);
    std::snprintf(out + used, capacity - used, "\nNext: DIAG %u",
                  static_cast<unsigned>(page + 1));
  }
  return true;
}
} // namespace cca_diagnostic_text
