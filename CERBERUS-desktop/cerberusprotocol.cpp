#include "cerberusprotocol.h"

CerberusProtocol::CerberusProtocol(QObject *parent)
    : QObject(parent)
{}

void CerberusProtocol::feedBytes(const QByteArray &data)
{
    const uint8_t *ptr = reinterpret_cast<const uint8_t *>(data.constData());
    buffer.insert(buffer.end(), ptr, ptr + data.size());
    parse_frame(); // operates on member buffer
}

void CerberusProtocol::discard_n(size_t n)
{
    if (n > buffer.size()) {
        buffer.clear();
        emit protocolError();
        return;
    }

    buffer.erase(buffer.begin(), buffer.begin() + n);
}

void CerberusProtocol::parse_frame()
{
    if (buffer.size() > MAX_BUFFER) {
        buffer.clear();
        emit protocolError();
        return;
    }

    while (true) {
        auto it = std::find(buffer.begin(), buffer.end(), START_BYTE);

        // Check if element is present
        if (it == buffer.end()) {
            buffer.clear();
            break;
        } else if (buffer.at(0) != START_BYTE) {
            discard_n(std::distance(buffer.begin(), it));
        }

        if (buffer.size() < 3) {
            break;
        }

        uint8_t frame_len = buffer.at(2);
        if (buffer.size() < frame_len + 4) {
            break;
        }

        uint8_t check = crbrs_CRC8(&buffer.at(1), frame_len + 2);
        if (buffer.at(frame_len + 3) == check) {
            QByteArray payload(reinterpret_cast<const char *>(buffer.data() + 3), frame_len);
            emit frameReceived(buffer.at(1), payload);
            discard_n(frame_len + 4); //discard this frame
        } else {
            discard_n(1); //discard extra START_BYTE
        }
    }
}

void CerberusProtocol::send_packet(uint8_t opcode, const std::vector<uint8_t> &payload)
{
    if (payload.size() > MAX_PAYLOAD) {
        emit protocolError();
        return;
    }
    uint8_t len = static_cast<uint8_t>(payload.size());
    std::vector<uint8_t> packet = {START_BYTE, opcode, len};

    if (!payload.empty()) {
        packet.reserve(packet.size() + len + 2);
        packet.insert(packet.end(), payload.begin(), payload.end());
    }

    packet.push_back(crbrs_CRC8(&packet.at(1), len + 2));
    emit bytesToSend(QByteArray(reinterpret_cast<const char *>(packet.data()), packet.size()));
}

void CerberusProtocol::sendCommand(uint8_t opcode, const std::vector<uint8_t> &payload)
{
    send_packet(opcode, payload);
}
