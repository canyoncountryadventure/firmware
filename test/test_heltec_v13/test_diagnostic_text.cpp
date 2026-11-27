#include "src/modules/CcaDiagnosticText.h"
#include <cassert>
#include <iostream>
#include <string>

static size_t wireSize(size_t payload) {
  // Data.portnum=TEXT_MESSAGE_APP and Data.payload, plus radio and PKI headers.
  const size_t data = 2 + 1 + (payload < 128 ? 1 : 2) + payload;
  return 16 + 12 + data;
}

int main() {
  using namespace cca_diagnostic_text;
  assert(wireSize(229) == 262);
  assert(wireSize(229) > 255);
  assert(wireSize(MAX_REPLY_BYTES) <= 255);

  for (size_t length : {size_t(0), size_t(1), size_t(169), size_t(170),
                        size_t(171), size_t(340), size_t(699)}) {
    std::string text;
    for (size_t i = 0; i < length; ++i)
      text += static_cast<char>('a' + i % 26);
    const size_t pages = length ? (length + PAGE_BYTES - 1) / PAGE_BYTES : 1;
    std::string recovered;
    for (size_t page = 1; page <= pages; ++page) {
      char reply[MAX_REPLY_BYTES + 1] = {};
      const std::string argument = page == 1 ? "" : std::to_string(page);
      assert(formatPage(text.c_str(), argument.c_str(), reply, sizeof(reply)));
      assert(std::strlen(reply) <= MAX_REPLY_BYTES);
      assert(wireSize(std::strlen(reply)) <= 255);
      const std::string prefix =
          "DIAG " + std::to_string(page) + "/" + std::to_string(pages) + "\n";
      assert(std::string(reply).compare(0, prefix.size(), prefix) == 0);
      const size_t count = std::min(PAGE_BYTES, length - recovered.size());
      recovered.append(reply + prefix.size(), count);
      if (page < pages)
        assert(std::string(reply).find("\nNext: DIAG " +
                                       std::to_string(page + 1)) !=
               std::string::npos);
      else
        assert(std::string(reply).find("Next:") == std::string::npos);
    }
    assert(recovered == text);
    for (const char *bad : {"0", "-1", "99", "2x", "NETWORK",
                            "9999999999999999999999999999999"}) {
      char reply[MAX_REPLY_BYTES + 1] = {};
      assert(!formatPage(text.c_str(), bad, reply, sizeof(reply)));
      assert(std::strlen(reply) <= MAX_REPLY_BYTES);
      assert(std::string(reply).find("Use DIAG") == 0);
    }
  }
  char small[4] = {'x', 'x', 'x', '\0'};
  assert(!formatPage("sample", "", small, sizeof(small)));
  assert(std::string(small) == "xxx");
  std::cout
      << "PASS: old reply exceeds PKI limit (262/255 bytes); all diagnostic "
         "pages fit, preserve full text and reject invalid pages\n";
}
