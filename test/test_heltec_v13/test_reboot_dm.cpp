#include "src/modules/CcaGatewayRebootDm.h"
#include <cassert>
#include <cstring>
#include <cstdio>
using namespace cca_gateway_reboot;
static bool parse(const char *s) { return isCommand(reinterpret_cast<const uint8_t *>(s), strlen(s)); }
int main() {
    assert(parse("reboot")); assert(parse("REBOOT")); assert(parse(" \tReBoOt\r\n"));
    assert(!parse("please reboot")); assert(!parse("REBOOT NOW")); assert(!parse("REBOOTED"));
    assert(!parse("")); assert(!parse("  ")); assert(!isCommand(nullptr,0));
    const uint8_t invalid[]={'R','E','B','O','O','T',0,'x'};
    assert(!isCommand(invalid,sizeof(invalid)));
    assert(authorized(true,true,true,true,false));
    assert(!authorized(true,true,true,false,true));
    assert(authorized(true,true,false,false,true));
    assert(!authorized(true,true,false,false,false));
    assert(!authorized(false,true,true,true,true));
    assert(!authorized(true,false,true,true,true));
    std::puts("PASS: exact REBOOT DM and fail-closed authorization scenarios");
}
