#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cryptoauthlib.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "led_strip.h"
#include "sdkconfig.h"
#include "opcodes.h"
#include "protocol.h"
#include "transport.h"
#include "nvs_flash.h"
#include "store.h"

static const char *TAG = "CERBERUS";

#define BLINK_GPIO CONFIG_BLINK_GPIO
#define UART_NUM UART_NUM_0
#define UART_TX_PIN 43
#define UART_RX_PIN 44
#define BUF_SIZE 256

static led_strip_handle_t led_strip;
// ── LED

static void configure_led(void)
{
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

static void led_set_color(uint8_t r, uint8_t g, uint8_t b)
{
    led_strip_set_pixel(led_strip, 0, r, g, b);
    led_strip_refresh(led_strip);
}

static void led_off(void)
{
    led_strip_clear(led_strip);
}

// ── PACKETS

static inline size_t crbrs_encode_bits(uint8_t buf[MAX_PAYLOAD], size_t pos, uint16_t block)
{
    buf[pos++] = block & 0xFF;
    buf[pos++] = block >> 8;
    return pos;
}

static inline size_t crbrs_encode_char(uint8_t buf[MAX_PAYLOAD], size_t pos, const char *text, const size_t cap)
{
    size_t len = strlen(text);
    if (len > cap)
    {
        return 0;
    }
    buf[pos++] = len;
    memcpy(&buf[pos], text, len);
    pos += len;
    return pos;
}

// packs a metadata struct to be sent later
static inline size_t crbrs_pack_metadata(const metadata *metadata, uint8_t buf[MAX_PAYLOAD], size_t buf_len)
{
    size_t pos = 0;
    if (buf_len < MAX_PAYLOAD)
        return 0;

    pos = crbrs_encode_bits(buf, pos, metadata->slot_idx);
    pos = crbrs_encode_bits(buf, pos, metadata->time_modified);
    pos = crbrs_encode_bits(buf, pos, metadata->time_created);
    buf[pos++] = metadata->flags; // pos = 8

    if (pos + 1 + CAP_SITE > buf_len)
        return 0;
    pos = crbrs_encode_char(buf, pos, metadata->site, CAP_SITE);
    if (pos == 0)
        return 0;

    if (pos + 1 + CAP_URL > buf_len)
        return 0;
    pos = crbrs_encode_char(buf, pos, metadata->url, CAP_URL);
    if (pos == 0)
        return 0;

    if (pos + 1 + CAP_EMAIL > buf_len)
        return 0;
    pos = crbrs_encode_char(buf, pos, metadata->email, CAP_EMAIL);
    if (pos == 0)
        return 0;

    if (pos + 1 + CAP_NOTES > buf_len)
        return 0;
    pos = crbrs_encode_char(buf, pos, metadata->notes, CAP_NOTES);
    if (pos == 0)
        return 0;

    return pos;
}

static void crbrs_storage(const metadata *meta)
{
    uint8_t buf[MAX_PAYLOAD];
    size_t pos = crbrs_pack_metadata(meta, buf, sizeof(buf));
    if (pos != 0)
    {
        crbrs_send_packet(RES_METADATA, (uint8_t)pos, buf);
    }
}

// sends packets thru the wire based on specified actions
void crbrs_dispatch(uint8_t opcode, uint8_t len, const uint8_t *payload)
{
    uint8_t reason = NACK_UNKNOWN_OPCODE;
    switch (opcode)
    {
    case CMD_PING:
        ESP_LOGI(TAG, "received opcode: 0x%02X", opcode);
        crbrs_send_packet(RES_PONG, 0, NULL);

        // flash green
        led_set_color(0, 32, 0);
        vTaskDelay(pdMS_TO_TICKS(300));
        led_off();
        break;

    case CMD_UNLOCK:
    {
        esp_err_t err = crbrs_find_meta(crbrs_storage);
        if (err != ESP_OK)
        {
            reason = NACK_STORAGE_ERROR;
            crbrs_send_packet(RES_NACK, 1, &reason);
            break;
        }
        crbrs_send_packet(RES_METADATA_END, 0, NULL);
        break;
    }

    case CMD_GET_PASSWORD:
    {
        if (len != 2)
        {
            reason = NACK_BAD_PAYLOAD;
            crbrs_send_packet(RES_NACK, 1, &reason);
            break;
        }
        uint8_t buf[MAX_PAYLOAD];
        uint8_t low = payload[1];
        uint16_t slot_idx = payload[0] | (low << 8);
        metadata meta;
        secret pw;
        esp_err_t err = crbrs_read_meta(slot_idx, &meta);
        if (err == ESP_ERR_NVS_NOT_FOUND)
        {
            reason = NACK_BAD_SLOT_IDX;
            crbrs_send_packet(RES_NACK, 1, &reason);
            break;
        }
        else if (err != ESP_OK)
        {
            reason = NACK_STORAGE_ERROR;
            crbrs_send_packet(RES_NACK, 1, &reason);
            break;
        }
        if (!(meta.flags & FLAG_OCCUPIED))
        {
            reason = NACK_BAD_SLOT_IDX;
            crbrs_send_packet(RES_NACK, 1, &reason);
            break;
        }

        err = crbrs_read_pw(slot_idx, &pw);
        if (err == ESP_ERR_NVS_NOT_FOUND)
        {
            reason = NACK_BAD_SLOT_IDX;
            crbrs_send_packet(RES_NACK, 1, &reason);
            break;
        }
        else if (err != ESP_OK)
        {
            reason = NACK_STORAGE_ERROR;
            crbrs_send_packet(RES_NACK, 1, &reason);
            break;
        }
        buf[0] = pw.len;
        memcpy(buf + 1, pw.password, pw.len);
        crbrs_send_packet(RES_PASSWORD, pw.len + 1, buf);
        memset(pw.password, 0, sizeof(pw.password));
        memset(buf, 0, sizeof(buf));
        break;
    }

    case CMD_DELETE_CRED:
    {
        if (len != 2)
        {
            reason = NACK_BAD_PAYLOAD;
            crbrs_send_packet(RES_NACK, 1, &reason);
            break;
        }
        uint8_t low = payload[1];
        uint16_t slot_idx = payload[0] | (low << 8);
        metadata meta;

        esp_err_t err = crbrs_read_meta(slot_idx, &meta);
        if (err == ESP_ERR_NVS_NOT_FOUND)
        {
            reason = NACK_BAD_SLOT_IDX;
            crbrs_send_packet(RES_NACK, 1, &reason);
            break;
        }
        else if (err != ESP_OK)
        {
            reason = NACK_STORAGE_ERROR;
            crbrs_send_packet(RES_NACK, 1, &reason);
            break;
        }
        if (!(meta.flags & FLAG_OCCUPIED))
        {
            reason = NACK_BAD_SLOT_IDX;
            crbrs_send_packet(RES_NACK, 1, &reason);
            break;
        }

        //need to add allocator that overwrites if occupied flag is not there
        err = crbrs_delete(slot_idx);
        if (err != ESP_OK)
        {
            reason = NACK_STORAGE_ERROR;
            crbrs_send_packet(RES_NACK, 1, &reason);
            break;
        }
        crbrs_send_packet(RES_OK, 0, NULL);
        break;
    }

    default:
        ESP_LOGI(TAG, "unrecognized opcode: %02X", opcode);
        reason = NACK_UNKNOWN_OPCODE;
        crbrs_send_packet(RES_NACK, 1, &reason);

        // flash red
        led_set_color(32, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(200));
        led_off();
        break;
    }
}

// ── UART

static void uart_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

// ── UART TASK

void uart_task(void *pvParameters)
{
    uint8_t buf[MAX_BUFFER];
    frame_parser fp = {0};

    ESP_LOGI(TAG, "CERBERUS device ready - waiting for commands");

    while (1)
    {
        int len = uart_read_bytes(UART_NUM, buf, MAX_BUFFER - fp.buf_len,
                                  pdMS_TO_TICKS(20));
        if (len > 0)
        {
            crbrs_parse_frame(&fp, buf, len, crbrs_dispatch);
        }
    }
}

// ── ENTRY POINT

void app_main(void)
{
    configure_led();
    uart_init();
    esp_err_t err = atcab_init(&cfg_ateccx08a_i2c_default);
    ESP_LOGI(TAG, "ATECC608b: %s", esp_err_to_name(err));
    // ESP_ERROR_CHECK(err);
    err = crbrs_store_init();
    ESP_ERROR_CHECK(err);
    ESP_LOGI(TAG, "NVS: %s", esp_err_to_name(err));

    uint16_t slot_idx = 32;
    metadata cred2 = {};
    memset(&cred2, 0xAA, sizeof(cred2));

    err = crbrs_read_meta(slot_idx, &cred2);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        metadata cred1 = {slot_idx, 753, 752, 1, "github", "github.com", "example@gmail.com", "notes3"};

        err = crbrs_write_meta(slot_idx, &cred1);
        ESP_ERROR_CHECK(err);
        ESP_LOGI(TAG, "NVS: %s", esp_err_to_name(err));
        ESP_LOGI(TAG, "NVS: cred1[ Slot: %u ,mTime: %u ,cTime: %u ,flag: %u ,site: %s , url: %s, email: %s, notes: %s ]", cred1.slot_idx, cred1.time_modified, cred1.time_created, cred1.flags, cred1.site, cred1.url, cred1.email, cred1.notes);

        metadata cred3 = {132, 753, 752, 0, "claude", "claude.ai", "example@outlook.com", "note123213"};
        err = crbrs_write_meta(cred3.slot_idx, &cred3);
        ESP_ERROR_CHECK(err);
        ESP_LOGI(TAG, "NVS: %s", esp_err_to_name(err));

        metadata cred4 = {1, 755, 722, 1, "google", "google.com", "example@gmail.com", "no2tes3"};
        err = crbrs_write_meta(cred4.slot_idx, &cred4);
        ESP_ERROR_CHECK(err);
        ESP_LOGI(TAG, "NVS: %s", esp_err_to_name(err));
    }
    else
    {
        ESP_ERROR_CHECK(err);
        ESP_LOGI(TAG, "NVS: %s", esp_err_to_name(err));
        ESP_LOGI(TAG, "NVS: cred2[ Slot: %u ,mTime: %u ,cTime: %u ,flag: %u ,site: %s , url: %s, email: %s, notes: %s ]", cred2.slot_idx, cred2.time_modified, cred2.time_created, cred2.flags, cred2.site, cred2.url, cred2.email, cred2.notes);
    }

    secret pass2 = {};
    memset(&pass2, 0xAA, sizeof(pass2));
    err = crbrs_read_pw(slot_idx, &pass2);

    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        secret pass1 = {0};
        pass1.len = 10;
        memcpy(pass1.password, "asaasddasd", pass1.len);
        err = crbrs_write_pw(slot_idx, &pass1);
        ESP_ERROR_CHECK(err);
        ESP_LOGI(TAG, "NVS: %s", esp_err_to_name(err));
        ESP_LOGI(TAG, "NVS: pass1[ len: %u ,password: %.*s ]", pass1.len, pass1.len, pass1.password);
    }
    else
    {
        ESP_ERROR_CHECK(err);
        ESP_LOGI(TAG, "NVS: %s", esp_err_to_name(err));
        ESP_LOGI(TAG, "NVS: pass2[ len: %u ,password: %.*s ]", pass2.len, pass2.len, pass2.password);
    }

    ESP_LOGI(TAG, "NVS: calling find...");
    crbrs_find_meta(crbrs_storage);

    // ESP_LOGI(TAG, "NVS: %s (deleted)", esp_err_to_name(err));
    // ESP_ERROR_CHECK(err);

    // green on startup to show device is alive
    led_set_color(0, 32, 0);
    vTaskDelay(pdMS_TO_TICKS(150));
    led_off();

    xTaskCreate(uart_task, "uart_task", 4096, NULL, 5, NULL);
}