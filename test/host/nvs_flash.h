#ifndef NVS_FLASH_H
#define NVS_FLASH_H

/*
 * Minimal host-test stub for nvs_flash API.
 * Provides placeholder types and no-op functions for agent.c memory injection.
 */

#include "mock_esp.h"
#include <stddef.h>
#include <stdint.h>

typedef uint32_t nvs_handle_t;
typedef void *nvs_iterator_t;

typedef enum {
    NVS_READONLY = 0,
    NVS_READWRITE = 1,
} nvs_open_mode_t;

typedef enum {
    NVS_TYPE_STR = 0x21,
} nvs_type_t;

typedef struct {
    char namespace_name[16];
    char key[16];
    nvs_type_t type;
} nvs_entry_info_t;

static inline esp_err_t nvs_open(const char *ns, nvs_open_mode_t mode, nvs_handle_t *handle)
{
    (void)ns; (void)mode; (void)handle;
    return ESP_ERR_NOT_FOUND;
}

static inline void nvs_close(nvs_handle_t handle)
{
    (void)handle;
}

static inline esp_err_t nvs_entry_find_in_handle(nvs_handle_t handle, nvs_type_t type, nvs_iterator_t *it)
{
    (void)handle; (void)type;
    if (it) *it = NULL;
    return ESP_ERR_NOT_FOUND;
}

static inline void nvs_entry_info(nvs_iterator_t it, nvs_entry_info_t *info)
{
    (void)it; (void)info;
}

static inline esp_err_t nvs_entry_next(nvs_iterator_t *it)
{
    (void)it;
    return ESP_ERR_NOT_FOUND;
}

static inline void nvs_release_iterator(nvs_iterator_t it)
{
    (void)it;
}

static inline esp_err_t nvs_get_str(nvs_handle_t handle, const char *key, char *out, size_t *len)
{
    (void)handle; (void)key; (void)out; (void)len;
    return ESP_ERR_NOT_FOUND;
}

#endif // NVS_FLASH_H
