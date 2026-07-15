#include "transport.h"

#define UART_NUM        UART_NUM_0

void discard_n(frame_parser *fp, size_t n){
    if(n > fp->buf_len){
        fp->buf_len = 0;
        return;
    }
    
    //shift up the buffer n spaces
    memmove(&fp->buffer[0], &fp->buffer[n], fp->buf_len - n);
    fp->buf_len = fp->buf_len - n;
}

void crbrs_send_packet(uint8_t opcode, uint8_t len ,const uint8_t *payload){
    uint8_t packet[MAX_BUFFER] = {START_BYTE, opcode, len};
    if(len>0 && payload != NULL){
        for (size_t i = 0; i < len; i++)
        {
            packet[3+i] = payload[i]; //copy payload into packet
        }
    }
    packet[len+3] = crbrs_CRC8(&packet[1], len+2);

    uart_write_bytes(UART_NUM, packet, len + 4);
}

void crbrs_parse_frame(frame_parser *fp, const uint8_t *data, size_t len, frame_handler on_frame){
    if(((fp->buf_len) + len) > MAX_BUFFER ){
        fp->buf_len = 0; //drop the buffer
        return;
    }
    
    for (size_t i = 0; i < len; i++) //copy data into buffer
    {
        fp->buffer[(fp->buf_len) + i] = data[i];
    }
    fp->buf_len = (fp->buf_len) + len;
    
    while(true){
        bool none = true;
        
        if(fp->buffer[0] != START_BYTE){ //resync
            for (size_t i = 0; i < fp->buf_len; i++)
            {
                if(fp->buffer[i] == START_BYTE){
                    discard_n(fp, i);
                    none = false; 
                    break;
                }
            }
            if(none){ //flag if no start bytes at all
                fp->buf_len = 0;
                break;
            }
        }

        if(fp->buf_len < 3){
            break;
        }
        
        uint8_t frame_len = fp->buffer[2];
        if(fp->buf_len < frame_len + 4){
            break;
        }

        uint8_t check = crbrs_CRC8(&(fp->buffer[1]), frame_len + 2);
        if(fp->buffer[frame_len + 3] == check){
            on_frame(fp->buffer[1], frame_len, &fp->buffer[3]);//dispatch callback
            discard_n(fp, frame_len + 4); //go to next frame
        }else{
            discard_n(fp, 1); //delete consecutive START_BYTES
        }
    }
}