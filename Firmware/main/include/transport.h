#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <stdio.h>
#include <stdint.h>
#include "protocol.h"
#include "driver/uart.h"

typedef struct
{
    uint8_t buffer[MAX_BUFFER];
    size_t buf_len;
} frame_parser;

typedef void (*frame_handler)(uint8_t opcode, uint8_t len, const uint8_t *payload);

void discard_n(frame_parser *fp, size_t n);
void crbrs_send_packet(uint8_t opcode, uint8_t len, const uint8_t *payload);
void crbrs_parse_frame(frame_parser *fp, const uint8_t *data, size_t len, frame_handler on_frame);

#endif