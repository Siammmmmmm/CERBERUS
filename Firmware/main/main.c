#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "led_strip.h"
#include "sdkconfig.h"
#include "opcodes.h"
#include "protocol.h"

static const char *TAG = "CERBERUS";

#define BLINK_GPIO CONFIG_BLINK_GPIO
#define UART_NUM        UART_NUM_0
#define UART_TX_PIN     43
#define UART_RX_PIN     44
#define BUF_SIZE        256

static uint8_t s_led_state = 0;
static led_strip_handle_t led_strip;

typedef struct{
    uint8_t buffer[MAX_BUFFER];
    size_t buf_len;
} frame_parser;

// ── LED

static void configure_led(void) {
    ESP_LOGI(TAG, "Configuring addressable LED");
    led_strip_config_t strip_config = {
        .strip_gpio_num = BLINK_GPIO,
        .max_leds = 1,
    };

#if CONFIG_BLINK_LED_STRIP_BACKEND_RMT
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
#elif CONFIG_BLINK_LED_STRIP_BACKEND_SPI
    led_strip_spi_config_t spi_config = {
        .spi_bus = SPI2_HOST,
        .flags.with_dma = true,
    };
    ESP_ERROR_CHECK(led_strip_new_spi_device(&strip_config, &spi_config, &led_strip));
#else
#error "unsupported LED strip backend"
#endif
    led_strip_clear(led_strip);
}

static void led_set_color(uint8_t r, uint8_t g, uint8_t b) {
    led_strip_set_pixel(led_strip, 0, r, g, b);
    led_strip_refresh(led_strip);
}

static void led_off(void) {
    led_strip_clear(led_strip);
}


// ── PACKETS

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

void crbrs_dispatch(uint8_t opcode, uint8_t len, const uint8_t *payload){

    switch (opcode)
    {
    case CMD_PING:
        ESP_LOGI(TAG, "recieved opcode: 0x%02X", opcode);
        crbrs_send_packet(RES_PONG, 0, NULL);

        //flash green
        led_set_color(0, 32, 0);
        vTaskDelay(pdMS_TO_TICKS(200));
        led_off();
        break;

    default:
        ESP_LOGI(TAG, "unrecognized opcode: %02X", opcode);
        crbrs_send_packet(RES_NACK, 0, NULL);

        //flash red
        led_set_color(32, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(200));
        led_off();
        break;
    }

}

void crbrs_parse_frame(frame_parser *fp, const uint8_t *data, size_t len){
    if(((fp->buf_len) + len) > MAX_BUFFER ){
        fp->buf_len = 0; //reset len idx
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
            crbrs_dispatch(fp->buffer[1], frame_len, &fp->buffer[3]);
            discard_n(fp, frame_len + 4); //go to next frame
        }else{
            discard_n(fp, 1); //delete consecutive START_BYTES
        }
    }
}

// ── UART

static void uart_init(void) {
    uart_config_t uart_config = {
        .baud_rate  = 115200,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}


// ── UART TASK

void uart_task(void *pvParameters) {
    uint8_t buf[MAX_BUFFER];
    frame_parser fp = {0};

    ESP_LOGI(TAG, "CERBERUS device ready — waiting for commands");

    while (1) {
        int len = uart_read_bytes(UART_NUM, buf, MAX_BUFFER - fp.buf_len,
                                  pdMS_TO_TICKS(20));
        if (len>0)
        {
            crbrs_parse_frame(&fp, buf, len);
        }
    }
}

// ── ENTRY POINT

void app_main(void) {
    configure_led();
    uart_init();

    // blue on startup to show device is alive
    led_set_color(0, 0, 32);
    vTaskDelay(pdMS_TO_TICKS(150));
    led_off();

    xTaskCreate(uart_task, "uart_task", 4096, NULL, 5, NULL);
}