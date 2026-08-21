#include "store.h"
#include <stdio.h>
#include "nvs_flash.h"
#include "string.h"
#include "stdlib.h"

static const char *STORE_NAME = "credentials";

esp_err_t crbrs_store_init()
{
    // Confirmed: 16,128 entries so can hold over 1000 credential blocks
    esp_err_t err = nvs_flash_init_partition("cred");
    if ((err == ESP_ERR_NVS_NO_FREE_PAGES) || (err == ESP_ERR_NVS_NEW_VERSION_FOUND))
    {
        nvs_flash_erase_partition("cred");
        err = nvs_flash_init_partition("cred");
    }
    return err;
}

static esp_err_t get_key(char *key, int key_size, const char *prefix, uint16_t slot_idx)
{
    int size = snprintf(key, key_size, "%s:%d", prefix, slot_idx);
    if (size < 0)
    {
        return ESP_FAIL;
    }
    else if (size >= key_size)
    {
        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}

esp_err_t crbrs_read_meta(uint16_t slot_idx, metadata *storage)
{
    char key[16];
    nvs_handle_t handle;
    size_t storage_len = sizeof(*storage);
    esp_err_t err = get_key(key, sizeof(key), "meta", slot_idx);
    if (err != ESP_OK)
    {
        return err;
    }
    err = nvs_open_from_partition("cred", STORE_NAME, NVS_READONLY, &handle);
    if (err != ESP_OK)
    {
        return err;
    }
    err = nvs_get_blob(handle, key, storage, &storage_len);
    if (err != ESP_OK)
    {
        nvs_close(handle);
        return err;
    }
    if (storage_len != sizeof(*storage))
    {
        nvs_close(handle);
        return ESP_ERR_INVALID_SIZE;
    }
    nvs_close(handle);
    return err;
}
esp_err_t crbrs_write_meta(uint16_t slot_idx, const metadata *storage)
{
    char key[16];
    nvs_handle_t handle;
    esp_err_t err = get_key(key, sizeof(key), "meta", slot_idx);
    if (err != ESP_OK)
    {
        return err;
    }
    err = nvs_open_from_partition("cred", STORE_NAME, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        return err;
    }
    err = nvs_set_blob(handle, key, storage, sizeof(*storage));
    if (err != ESP_OK)
    {
        nvs_close(handle);
        return err;
    }
    err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

esp_err_t crbrs_read_pw(uint16_t slot_idx, secret *storage)
{
    char key[16];
    nvs_handle_t handle;
    size_t storage_len = sizeof(*storage);
    esp_err_t err = get_key(key, sizeof(key), "pw", slot_idx);
    if (err != ESP_OK)
    {
        return err;
    }
    err = nvs_open_from_partition("cred", STORE_NAME, NVS_READONLY, &handle);
    if (err != ESP_OK)
    {
        return err;
    }
    err = nvs_get_blob(handle, key, storage, &storage_len);
    if (err != ESP_OK)
    {
        nvs_close(handle);
        return err;
    }
    if (storage_len != sizeof(*storage))
    {
        nvs_close(handle);
        return ESP_ERR_INVALID_SIZE;
    }
    nvs_close(handle);
    return err;
}
esp_err_t crbrs_write_pw(uint16_t slot_idx, const secret *storage)
{
    char key[16];
    nvs_handle_t handle;
    esp_err_t err = get_key(key, sizeof(key), "pw", slot_idx);
    if (err != ESP_OK)
    {
        return err;
    }
    err = nvs_open_from_partition("cred", STORE_NAME, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        return err;
    }
    err = nvs_set_blob(handle, key, storage, sizeof(*storage));
    if (err != ESP_OK)
    {
        nvs_close(handle);
        return err;
    }
    err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

esp_err_t crbrs_delete(uint16_t slot_idx)
{
    char key[16];
    nvs_handle_t handle;
    esp_err_t err = get_key(key, sizeof(key), "pw", slot_idx);
    if (err != ESP_OK)
    {
        return err;
    }
    err = nvs_open_from_partition("cred", STORE_NAME, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        return err;
    }
    err = nvs_erase_key(handle, key);
    if ((err != ESP_ERR_NVS_NOT_FOUND) && (err != ESP_OK))
    {
        nvs_close(handle);
        return err;
    }
    err = get_key(key, sizeof(key), "meta", slot_idx);
    if (err != ESP_OK)
    {
        nvs_close(handle);
        return err;
    }
    err = nvs_erase_key(handle, key);
    if ((err != ESP_ERR_NVS_NOT_FOUND) && (err != ESP_OK))
    {
        nvs_close(handle);
        return err;
    }
    err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

esp_err_t crbrs_find_meta(storage_handler storage)
{
    nvs_iterator_t it = NULL;
    nvs_entry_info_t info;
    metadata meta;
    esp_err_t err = nvs_entry_find("cred", STORE_NAME, NVS_TYPE_BLOB, &it);
    while (true)
    {
        if (err == ESP_ERR_NVS_NOT_FOUND)
        {
            break;
        }
        err = nvs_entry_info(it, &info);
        if (err != ESP_OK)
        {
            break;
        }
        int cmp = strncmp(info.key, "meta:", 5);
        if (cmp != 0)
        {
            goto next;
        }
        err = crbrs_read_meta(atoi(info.key + 5), &meta);
        if (err != ESP_OK)
        {
            goto next;
        }
        if (meta.flags & FLAG_OCCUPIED)
        {
            storage(&meta);
        }
    next:
        err = nvs_entry_next(&it);
    }

    if (it != NULL)
    {
        nvs_release_iterator(it);
    }
    return ESP_OK;
}
