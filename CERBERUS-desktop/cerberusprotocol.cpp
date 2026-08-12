#include "cerberusprotocol.h"

CerberusProtocol::CerberusProtocol(QObject *parent)
    : QObject(parent)
{}

quint16 CerberusProtocol::readU16(const QByteArray &payload, size_t &pos)
{//format 2 8bit blocks into 1 16bit block
    quint8 high = static_cast<quint8>(payload.at(pos + 1)) & 0xFF;
    quint16 block = (static_cast<quint8>(payload.at(pos)) | (high << 8));
    pos += 2;
    return block;
}

QDate CerberusProtocol::decode_date(quint16 block)
{
    return QDate((block >> 9) + 2025, (block >> 5) & 0x0F, block & 0x1F);
}

QString CerberusProtocol::decode_char(size_t &pos,
                                      const QByteArray &payload,
                                      const size_t cap,
                                      bool &ok)
{
    size_t len = static_cast<quint8>(payload.at(pos));
    QString info;
    pos++;
    if ((len > cap) || ((pos + len) > payload.size())) {
        ok = false;
    } else {
        info = QString::fromUtf8(payload.constData() + pos, len);
        pos += len;
    }
    return info;
};

bool CerberusProtocol::unpack_metadata(const QByteArray &payload, credential &metadata)
{
    size_t pos = 0;
    bool ok = true;

    if (payload.size() < 9) {
        return false;
    }

    metadata.slot_idx = readU16(payload, pos);

    metadata.time_accessed = decode_date(readU16(payload, pos));
    metadata.time_modified = decode_date(readU16(payload, pos));
    metadata.time_created = decode_date(readU16(payload, pos));

    metadata.flags = static_cast<quint8>(payload.at(pos));
    pos++;

    metadata.site = decode_char(pos, payload, CAP_SITE, ok);
    if (!ok) {
        return false;
    }
    metadata.url = decode_char(pos, payload, CAP_URL, ok);
    if (!ok) {
        return false;
    }
    metadata.email = decode_char(pos, payload, CAP_EMAIL, ok);
    if (!ok) {
        return false;
    }
    metadata.notes = decode_char(pos, payload, CAP_NOTES, ok);
    if (pos != payload.size() || !ok) {
        return false;
    }

    return true;
};

bool CerberusProtocol::unpack_password(const QByteArray &payload, QString &password){
    size_t pos = 0;
    bool ok = true;

    password = decode_char(pos, payload, CAP_PASSWORD, ok);
    if (pos != payload.size() || !ok) {
        return false;
    }
    return true;
}

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
        if (buffer.empty()) {
            break;
        }

        // Check if START_BYTE is present if not, delete everything til there is one
        if (buffer.at(0) != START_BYTE) {
            auto it = std::find(buffer.begin(), buffer.end(), START_BYTE);
            if (it == buffer.end()) {
                buffer.clear();
                break;
            }
            discard_n(std::distance(buffer.begin(), it));
        }

        if (buffer.size() < 3) {
            break;
        }

        uint8_t frame_len = buffer.at(2);
        if (buffer.size() < frame_len + 4) { //frame_len is int
            break;
        }

        uint8_t check = crbrs_CRC8(&buffer.at(1), frame_len + 2); //validate with crc
        if (buffer.at(frame_len + 3) == check) {
            QByteArray payload(reinterpret_cast<const char *>(buffer.data() + 3), frame_len);
            uint8_t opcode = buffer.at(1); //seperate payload and opcode
            switch (opcode) {
            case RES_METADATA: {
                credential metadata;
                if (CerberusProtocol::unpack_metadata(payload, metadata)) {
                    emit metadataReceived(metadata);
                } else {
                    emit protocolError();
                }
                break;
            }

            case RES_METADATA_END:
                emit metadataComplete();
                break;

            case RES_PASSWORD:
            {
                QString password;
                if (CerberusProtocol::unpack_password(payload, password)) {
                    emit passReceived(password);
                } else {
                    emit protocolError();
                }
                break;
            }
            case RES_NACK:
                emit nackReceived(static_cast<quint8>(payload.at(0)));
                break;

            default:
                emit frameReceived(opcode, payload);

                break;
            }
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
