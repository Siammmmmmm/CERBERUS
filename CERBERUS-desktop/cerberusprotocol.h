#ifndef CERBERUSPROTOCOL_H
#define CERBERUSPROTOCOL_H

#include <QByteArray>
#include <QObject>
#include "opcodes.h"
#include "protocol.h"
#include <cstdint>
#include <qdatetime.h>
#include <vector>

struct credential
{
    quint16 slot_idx;
    QDate time_accessed;
    QDate time_modified;
    QDate time_created;
    quint8 flags;
    QString site;
    QString url;
    QString email;
    QString notes;
};

class CerberusProtocol : public QObject
{
    Q_OBJECT

public:
    explicit CerberusProtocol(QObject *parent = nullptr);
    void feedBytes(const QByteArray &data);
    void sendCommand(uint8_t opcode, const std::vector<uint8_t> &payload);

signals:
    void frameReceived(uint8_t opcode, QByteArray payload);
    void metadataReceived(credential metadata);
    void metadataComplete();
    void bytesToSend(QByteArray data);
    void protocolError();

private:
    std::vector<uint8_t> buffer;
    bool unpack_metadata(const QByteArray &payload, credential &metadata);
    QString decode_char(size_t &pos, const QByteArray &payload, const size_t cap, bool &ok);
    quint16 readU16(const QByteArray &payload, size_t &pos);
    QDate decode_date(quint16 block);
    void parse_frame();
    void send_packet(uint8_t opcode, const std::vector<uint8_t> &payload);
    void discard_n(size_t n);
};

#endif // CERBERUSPROTOCOL_H
