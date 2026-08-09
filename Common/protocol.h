// protocol.h
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#define START_BYTE 0xAA
#define MAX_PAYLOAD 255
#define MAX_BUFFER (MAX_PAYLOAD+4)
#define CAP_SITE 32
#define CAP_URL 96
#define CAP_EMAIL 64
#define CAP_NOTES 50
#define CAP_PASSWORD 30
#define FLAG_OCCUPIED 0x1
#define FLAG_FAVORITE 0x2

//start byte NOT fed also answer is NOT reflected
static inline uint8_t crbrs_CRC8(const uint8_t *data, int len){
    uint8_t crc = 0x00; 
    for (int i = 0; i < len; i++)
    {
        crc ^= data[i]; //Gets byte at idx
        for (int x = 0; x < 8; x++)
        {
            if(crc & 0x80){
                crc = (crc << 1) ^ 0x31; //dallas/maxim poly
            }else{
                crc = crc << 1;
            }
        }
        
    }
    return crc;
}

#endif
