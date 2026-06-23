/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "led_strip.h"
#include "sdkconfig.h"

static const char *TAG = "cerberus";

#define BLINK_GPIO CONFIG_BLINK_GPIO
#define UART_NUM        UART_NUM_0
#define UART_TX_PIN     43
#define UART_RX_PIN     44
#define BUF_SIZE        256

static uint8_t s_led_state = 0;
static led_strip_handle_t led_strip;

// ── LED ──────────────────────────────────────────────────────────────────────

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

// ── UART ─────────────────────────────────────────────────────────────────────

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

// ── COMMAND HANDLER ───────────────────────────────────────────────────────────

static void process_command(const char *cmd) {
    ESP_LOGI(TAG, "Received command: %s", cmd);

    if (strcmp(cmd, "PING") == 0) {
        // flash green and respond
        led_set_color(0, 32, 0);
        vTaskDelay(pdMS_TO_TICKS(200));
        led_off();
        uart_write_bytes(UART_NUM, "PONG\n", 5);

    } else {
        // flash red for unknown command
        led_set_color(32, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(200));
        led_off();
        uart_write_bytes(UART_NUM, "UNKNOWN_CMD\n", 12);
    }
}

// ── UART TASK ─────────────────────────────────────────────────────────────────

void uart_task(void *pvParameters) {
    uint8_t buf[BUF_SIZE];
    char    cmd[BUF_SIZE];
    int     cmd_len = 0;

    uart_write_bytes(UART_NUM, "Cerberus device ready\n", 22);
    ESP_LOGI(TAG, "Cerberus device ready — waiting for commands");

    while (1) {
        int len = uart_read_bytes(UART_NUM, buf, sizeof(buf) - 1,
                                  pdMS_TO_TICKS(20));
        if (len > 0) {
            for (int i = 0; i < len; i++) {
                char c = (char)buf[i];
                if (c == '\n' || c == '\r') {
                    if (cmd_len > 0) {
                        cmd[cmd_len] = '\0';
                        process_command(cmd);
                        cmd_len = 0;
                    }
                } else if (cmd_len < BUF_SIZE - 1) {
                    cmd[cmd_len++] = c;
                }
            }
        }
    }
}

// ── ENTRY POINT ───────────────────────────────────────────────────────────────

void app_main(void) {
    configure_led();
    uart_init();

    // blue on startup to show device is alive
    led_set_color(0, 0, 32);
    vTaskDelay(pdMS_TO_TICKS(500));
    led_off();

    xTaskCreate(uart_task, "uart_task", 4096, NULL, 5, NULL);
}