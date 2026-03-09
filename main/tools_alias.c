#include "tools_handlers.h"
#include "config.h"
#include "memory.h"
#include "nvs_keys.h"
#include <stdio.h>
#include <string.h>

// Parse aliases string and find entry by name.
// Returns pointer to start of matching entry within buf, or NULL.
// Sets *entry_end to one past the entry (comma or NUL).
static char *find_alias_entry(char *buf, const char *name, char **entry_end)
{
    char *p = buf;
    size_t name_len = strlen(name);

    while (*p) {
        char *start = p;
        // Find end of this entry (next comma or NUL)
        char *comma = strchr(p, ',');
        char *end = comma ? comma : p + strlen(p);

        // Check if entry starts with "name:"
        size_t entry_len = (size_t)(end - start);
        if (entry_len > name_len && start[name_len] == ':' &&
            strncmp(start, name, name_len) == 0) {
            if (entry_end) *entry_end = end;
            return start;
        }

        if (!comma) break;
        p = comma + 1;
    }

    return NULL;
}

bool tools_set_alias_handler(const cJSON *input, char *result, size_t result_len)
{
    cJSON *name_json = cJSON_GetObjectItem(input, "name");
    cJSON *pin_json = cJSON_GetObjectItem(input, "pin");
    cJSON *state_json = cJSON_GetObjectItem(input, "state");

    if (!name_json || !cJSON_IsString(name_json) || name_json->valuestring[0] == '\0') {
        snprintf(result, result_len, "Error: 'name' required (non-empty string)");
        return false;
    }
    if (!pin_json || !cJSON_IsNumber(pin_json)) {
        snprintf(result, result_len, "Error: 'pin' required (integer)");
        return false;
    }

    const char *name = name_json->valuestring;
    int pin = pin_json->valueint;

    // Reject names containing delimiter characters
    if (strchr(name, ':') || strchr(name, ',')) {
        snprintf(result, result_len, "Error: alias name cannot contain ':' or ','");
        return false;
    }

    // Build the new entry: "name:pin" or "name:pin:state"
    char new_entry[64];
    if (state_json && cJSON_IsNumber(state_json)) {
        int state = state_json->valueint;
        snprintf(new_entry, sizeof(new_entry), "%s:%d:%d", name, pin, state);
    } else {
        snprintf(new_entry, sizeof(new_entry), "%s:%d", name, pin);
    }

    // Read current aliases
    char buf[DEV_ALIASES_MAX_LEN] = {0};
    memory_get(NVS_KEY_DEV_ALIASES, buf, sizeof(buf));

    // Remove existing entry with same name (if any)
    char *entry_end = NULL;
    char *entry = find_alias_entry(buf, name, &entry_end);
    if (entry) {
        // Shift remaining content over the old entry
        if (*entry_end == ',') entry_end++;  // skip trailing comma
        memmove(entry, entry_end, strlen(entry_end) + 1);
        // Remove trailing comma if we left one
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == ',') buf[len - 1] = '\0';
    }

    // Check combined length before assembling
    size_t buf_len = strlen(buf);
    size_t entry_len = strlen(new_entry);
    size_t need = buf_len + (buf_len ? 1 : 0) + entry_len;
    if (need >= DEV_ALIASES_MAX_LEN) {
        snprintf(result, result_len, "Error: aliases would exceed %d bytes", DEV_ALIASES_MAX_LEN);
        return false;
    }

    // Append new entry in-place
    if (buf_len) {
        buf[buf_len] = ',';
        memcpy(buf + buf_len + 1, new_entry, entry_len + 1);
    } else {
        memcpy(buf, new_entry, entry_len + 1);
    }

    esp_err_t err = memory_set(NVS_KEY_DEV_ALIASES, buf);
    if (err != ESP_OK) {
        snprintf(result, result_len, "Error: NVS write failed (%s)", esp_err_to_name(err));
        return false;
    }

    snprintf(result, result_len, "Alias set: %s", new_entry);
    return true;
}

bool tools_get_aliases_handler(const cJSON *input, char *result, size_t result_len)
{
    (void)input;

    char buf[DEV_ALIASES_MAX_LEN] = {0};
    if (!memory_get(NVS_KEY_DEV_ALIASES, buf, sizeof(buf)) || buf[0] == '\0') {
        snprintf(result, result_len, "No device aliases configured");
        return true;
    }

    // Format: one alias per line
    char *ptr = result;
    size_t remaining = result_len;
    int count = 0;

    char *p = buf;
    while (*p && remaining > 1) {
        char *comma = strchr(p, ',');
        size_t entry_len = comma ? (size_t)(comma - p) : strlen(p);

        if (count > 0) {
            int n = snprintf(ptr, remaining, "\n");
            if (n > 0) { ptr += n; remaining -= (size_t)n; }
        }

        int n = snprintf(ptr, remaining, "%.*s", (int)entry_len, p);
        if (n > 0) { ptr += n; remaining -= (size_t)n; }
        count++;

        if (!comma) break;
        p = comma + 1;
    }

    return true;
}

bool tools_delete_alias_handler(const cJSON *input, char *result, size_t result_len)
{
    cJSON *name_json = cJSON_GetObjectItem(input, "name");

    if (!name_json || !cJSON_IsString(name_json) || name_json->valuestring[0] == '\0') {
        snprintf(result, result_len, "Error: 'name' required (non-empty string)");
        return false;
    }

    const char *name = name_json->valuestring;

    char buf[DEV_ALIASES_MAX_LEN] = {0};
    if (!memory_get(NVS_KEY_DEV_ALIASES, buf, sizeof(buf)) || buf[0] == '\0') {
        snprintf(result, result_len, "Alias '%s' not found", name);
        return true;
    }

    char *entry_end = NULL;
    char *entry = find_alias_entry(buf, name, &entry_end);
    if (!entry) {
        snprintf(result, result_len, "Alias '%s' not found", name);
        return true;
    }

    // Remove entry
    if (*entry_end == ',') entry_end++;
    memmove(entry, entry_end, strlen(entry_end) + 1);
    // Remove trailing comma
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == ',') buf[len - 1] = '\0';

    if (buf[0] == '\0') {
        // All aliases removed, delete the key
        memory_delete(NVS_KEY_DEV_ALIASES);
    } else {
        esp_err_t err = memory_set(NVS_KEY_DEV_ALIASES, buf);
        if (err != ESP_OK) {
            snprintf(result, result_len, "Error: NVS write failed (%s)", esp_err_to_name(err));
            return false;
        }
    }

    snprintf(result, result_len, "Deleted alias: %s", name);
    return true;
}
