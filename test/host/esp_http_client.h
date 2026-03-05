#ifndef ESP_HTTP_CLIENT_H
#define ESP_HTTP_CLIENT_H

/*
 * Host-test stub for esp_http_client API.
 * Provides placeholder types and no-op function declarations so production
 * source files that reference the HTTP client API can compile and link.
 */

#include "mock_esp.h"
#include <stddef.h>

typedef void *esp_http_client_handle_t;

typedef enum {
    HTTP_EVENT_ON_DATA = 0,
} esp_http_client_event_id_t;

typedef struct {
    esp_http_client_event_id_t event_id;
    void *data;
    int data_len;
    void *user_data;
} esp_http_client_event_t;

typedef esp_err_t (*http_event_handle_cb)(esp_http_client_event_t *evt);

typedef enum {
    HTTP_METHOD_GET = 0,
    HTTP_METHOD_POST,
} esp_http_client_method_t;

typedef enum {
    HTTP_TRANSPORT_UNKNOWN = 0,
    HTTP_TRANSPORT_OVER_TCP,
    HTTP_TRANSPORT_OVER_SSL,
} esp_http_client_transport_t;

typedef esp_err_t (*crt_bundle_attach_fn)(void *);

typedef struct {
    const char *url;
    http_event_handle_cb event_handler;
    void *user_data;
    int timeout_ms;
    crt_bundle_attach_fn crt_bundle_attach;
    esp_http_client_method_t method;
} esp_http_client_config_t;

esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *config);
esp_err_t esp_http_client_perform(esp_http_client_handle_t client);
esp_err_t esp_http_client_open(esp_http_client_handle_t client, int content_length);
int esp_http_client_write(esp_http_client_handle_t client, const char *buffer, int len);
int esp_http_client_fetch_headers(esp_http_client_handle_t client);
int esp_http_client_read(esp_http_client_handle_t client, char *buffer, int len);
int esp_http_client_get_status_code(esp_http_client_handle_t client);
int esp_http_client_get_errno(esp_http_client_handle_t client);
esp_http_client_transport_t esp_http_client_get_transport_type(esp_http_client_handle_t client);
esp_err_t esp_http_client_set_method(esp_http_client_handle_t client, esp_http_client_method_t method);
esp_err_t esp_http_client_set_header(esp_http_client_handle_t client, const char *key, const char *value);
esp_err_t esp_http_client_set_post_field(esp_http_client_handle_t client, const char *data, int len);
esp_err_t esp_http_client_close(esp_http_client_handle_t client);
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t client);

#endif // ESP_HTTP_CLIENT_H
