#ifndef STORE_H
#define STORE_H

#include <stdint.h>
#include "protocol.h"
#include "esp_err.h"

#define IV_LEN 12
#define TAG_LEN 16

// structs written by memcpy so changing order or types breaks existing record!!
typedef struct
{
    uint16_t slot_idx;
    uint16_t time_modified;
    uint16_t time_created;
    uint8_t flags;
    char site[CAP_SITE + 1];
    char url[CAP_URL + 1];
    char email[CAP_EMAIL + 1];
    char notes[CAP_NOTES + 1];
} metadata;

// stores passwords in ciphertext
typedef struct
{
    uint8_t len;
    uint8_t password[CAP_PASSWORD];
    uint8_t iv[IV_LEN];
    uint8_t tag[TAG_LEN];
} secret;

esp_err_t crbrs_store_init();
esp_err_t crbrs_delete(uint16_t slot_idx);

esp_err_t crbrs_read_meta(uint16_t slot_idx, metadata *storage);
esp_err_t crbrs_write_meta(uint16_t slot_idx, const metadata *storage);

esp_err_t crbrs_read_pw(uint16_t slot_idx, secret *storage);
esp_err_t crbrs_write_pw(uint16_t slot_idx, const secret *storage);
typedef void (*storage_handler)(const metadata *meta);
esp_err_t crbrs_find_meta(storage_handler storage);

#endif