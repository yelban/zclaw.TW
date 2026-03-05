/*
 * Mock ESP-IDF functions for host testing
 */

#include "mock_esp.h"
#include "esp_http_client.h"
#include "esp_heap_caps.h"
#include "esp_crt_bundle.h"
#include <stdlib.h>
#include <stdio.h>

// --- esp_http_client stubs (never executed in host tests) ---

esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *config)
{
    (void)config;
    return NULL;
}

esp_err_t esp_http_client_perform(esp_http_client_handle_t client)
{
    (void)client;
    return ESP_FAIL;
}

esp_err_t esp_http_client_open(esp_http_client_handle_t client, int content_length)
{
    (void)client;
    (void)content_length;
    return ESP_FAIL;
}

int esp_http_client_write(esp_http_client_handle_t client, const char *buffer, int len)
{
    (void)client;
    (void)buffer;
    return len;
}

int esp_http_client_fetch_headers(esp_http_client_handle_t client)
{
    (void)client;
    return 0;
}

int esp_http_client_read(esp_http_client_handle_t client, char *buffer, int len)
{
    (void)client;
    (void)buffer;
    (void)len;
    return 0;
}

int esp_http_client_get_status_code(esp_http_client_handle_t client)
{
    (void)client;
    return 0;
}

int esp_http_client_get_errno(esp_http_client_handle_t client)
{
    (void)client;
    return 0;
}

esp_http_client_transport_t esp_http_client_get_transport_type(esp_http_client_handle_t client)
{
    (void)client;
    return HTTP_TRANSPORT_UNKNOWN;
}

esp_err_t esp_http_client_set_method(esp_http_client_handle_t client, esp_http_client_method_t method)
{
    (void)client;
    (void)method;
    return ESP_OK;
}

esp_err_t esp_http_client_set_header(esp_http_client_handle_t client, const char *key, const char *value)
{
    (void)client;
    (void)key;
    (void)value;
    return ESP_OK;
}

esp_err_t esp_http_client_set_post_field(esp_http_client_handle_t client, const char *data, int len)
{
    (void)client;
    (void)data;
    (void)len;
    return ESP_OK;
}

esp_err_t esp_http_client_close(esp_http_client_handle_t client)
{
    (void)client;
    return ESP_OK;
}

esp_err_t esp_http_client_cleanup(esp_http_client_handle_t client)
{
    (void)client;
    return ESP_OK;
}

// --- esp_heap_caps stubs ---

void *heap_caps_malloc(size_t size, uint32_t caps)
{
    (void)caps;
    return malloc(size);
}

uint32_t heap_caps_get_largest_free_block(uint32_t caps)
{
    (void)caps;
    return 1024 * 1024; // 1 MB placeholder
}

// --- esp_crt_bundle stub ---

esp_err_t esp_crt_bundle_attach(void *conf)
{
    (void)conf;
    return ESP_OK;
}
