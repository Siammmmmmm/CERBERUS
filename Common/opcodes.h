#ifndef OPCODES_H
#define OPCODES_H
//METADATA: slot_idx[0-1]; accessed[2-3]; modified[4-5]; created[6-7];
//flags[8]; len[9] site; len url; len email; notes;

#define CMD_PING          0x01
#define CMD_UNLOCK        0x02
#define CMD_GET_METADATA  0x03
#define CMD_GET_PASSWORD  0x04
#define CMD_ADD_CRED      0x05
#define CMD_DELETE_CRED   0x06
#define CMD_LOCK          0x07

#define RES_PONG          0x81
#define RES_METADATA      0x83
#define RES_PASSWORD      0x84

#define RES_METADATA_END  0x8D
#define RES_OK            0x8E
#define RES_NACK          0x8F

typedef enum {
    NACK_UNKNOWN_OPCODE = 0x00,
    NACK_DEVICE_LOCKED,        // auto numbers to 0x01
    NACK_BAD_SLOT_IDX,
    NACK_DECRYPT_FAILURE,
    NACK_STORAGE_ERROR,
    NACK_DEVICE_DISCONNECTED
} nack_reason;

#define EVT_LOG           0xC0 //log event for any 

#endif // OPCODES_H
