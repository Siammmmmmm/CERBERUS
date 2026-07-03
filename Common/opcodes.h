#ifndef OPCODES_H
#define OPCODES_H

#define CMD_PING          0x01
#define CMD_UNLOCK        0x02
#define CMD_GET_METADATA  0x03
#define CMD_GET_PASSWORD  0x04
#define CMD_ADD_CRED      0x05
#define CMD_DELETE_CRED   0x06
#define CMD_LOCK          0x07

#define RES_PONG          0x81
#define RES_OK            0x82
#define RES_NACK          0x83
#define RES_METADATA      0x84
#define RES_PASSWORD      0x85

#endif // OPCODES_H
