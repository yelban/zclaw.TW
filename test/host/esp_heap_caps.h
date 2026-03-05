#ifndef ESP_HEAP_CAPS_H
#define ESP_HEAP_CAPS_H

#include <stddef.h>
#include <stdint.h>

#define MALLOC_CAP_SPIRAM  (1 << 0)
#define MALLOC_CAP_8BIT    (1 << 1)

void *heap_caps_malloc(size_t size, uint32_t caps);
uint32_t heap_caps_get_largest_free_block(uint32_t caps);

#endif // ESP_HEAP_CAPS_H
