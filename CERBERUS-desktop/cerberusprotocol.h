#ifndef CERBERUSPROTOCOL_H
#define CERBERUSPROTOCOL_H

#include <QByteArray>
#include <QObject>
#include "opcodes.h"
#include "protocol.h"
#include <cstdint>
#include <vector>

class CerberusProtocol : public QObject
{
    Q_OBJECT

public:
    explicit CerberusProtocol(QObject *parent = nullptr);
    void feedBytes(const QByteArray &data);
    void sendCommand(uint8_t opcode, const std::vector<uint8_t> &payload);

signals:
    void frameReceived(uint8_t opcode, QByteArray payload);
    void bytesToSend(QByteArray data);
    void protocolError();

private:
    std::vector<uint8_t> buffer;
    void parse_frame();
    void send_packet(uint8_t opcode, const std::vector<uint8_t> &payload);
    void discard_n(size_t n);
};

#endif // CERBERUSPROTOCOL_H
