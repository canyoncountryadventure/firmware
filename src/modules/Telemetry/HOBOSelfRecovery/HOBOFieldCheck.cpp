#include "configuration.h"

#if defined(ARCH_NRF52) && (defined(SEEED_XIAO_NRF52840_KIT) || defined(RAK_4631))

#include "HOBOFieldCheck.h"

#include "MeshService.h"
#include "NodeDB.h"

#include <cctype>
#include <cstring>

namespace
{
bool isCommand(const uint8_t *bytes, size_t size, const char *expected)
{
    if (bytes == nullptr || size == 0 || expected == nullptr)
        return false;

    char command[32] = {};
    size_t n = size;
    if (n > sizeof(command) - 1)
        n = sizeof(command) - 1;
    memcpy(command, bytes, n);

    char *p = command;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        ++p;
    if (*p == '/')
        ++p;

    const size_t expectedLength = strlen(expected);
    for (size_t i = 0; i < expectedLength; ++i) {
        if (p[i] == '\0')
            return false;
        if (std::toupper(static_cast<unsigned char>(p[i])) !=
            std::toupper(static_cast<unsigned char>(expected[i])))
            return false;
    }

    p += expectedLength;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        ++p;

    return *p == '\0';
}
} // namespace

HOBOFieldCheckModule::HOBOFieldCheckModule()
    : SinglePortModule("hobo_field_check", meshtastic_PortNum_TEXT_MESSAGE_APP)
{
    isPromiscuous = true;
}

bool HOBOFieldCheckModule::wantPacket(const meshtastic_MeshPacket *p)
{
    return p != nullptr && p->decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP;
}

ProcessMessage HOBOFieldCheckModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (nodeDB == nullptr || mp.decoded.portnum != meshtastic_PortNum_TEXT_MESSAGE_APP)
        return ProcessMessage::CONTINUE;

    const uint32_t ourNode = nodeDB->getNodeNum();
    if (mp.to != ourNode || mp.from == ourNode)
        return ProcessMessage::CONTINUE;

    const uint8_t *payload = mp.decoded.payload.bytes;
    const size_t payloadSize = mp.decoded.payload.size;

    if (isCommand(payload, payloadSize, "AMY") || isCommand(payload, payloadSize, "CRESSTON")) {
        sendTextReply(mp.from, mp.channel, "is a little bitch");
    }

    return ProcessMessage::CONTINUE;
}

bool HOBOFieldCheckModule::sendTextReply(uint32_t destination, uint8_t channel, const char *text)
{
    if (text == nullptr || destination == 0)
        return false;

    meshtastic_MeshPacket *packet = allocDataPacket();
    if (packet == nullptr)
        return false;

    size_t len = strlen(text);
    if (len > sizeof(packet->decoded.payload.bytes))
        len = sizeof(packet->decoded.payload.bytes);

    memcpy(packet->decoded.payload.bytes, text, len);
    packet->decoded.payload.size = len;
    packet->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    packet->decoded.want_response = false;
    packet->to = destination;
    packet->channel = channel;
    packet->want_ack = true;
    packet->priority = meshtastic_MeshPacket_Priority_RELIABLE;

    service->sendToMesh(packet, RX_SRC_LOCAL, true);
    return true;
}

#endif
