#pragma once

#include <cstddef>
#include <cstdint>
#include <new>

#define MALLOC_CAP_DEFAULT 0
#define MALLOC_CAP_INTERNAL 0
#define MALLOC_CAP_SPIRAM 0
#define MALLOC_CAP_8BIT 0

inline void* heap_caps_malloc(size_t size, uint32_t) { return new (std::nothrow) uint8_t[size]; }
inline void heap_caps_free(void* buffer) { delete[] static_cast<uint8_t*>(buffer); }
inline size_t heap_caps_get_free_size(uint32_t) { return 0; }
inline size_t heap_caps_get_total_size(uint32_t) { return 0; }
inline size_t heap_caps_get_minimum_free_size(uint32_t) { return 0; }
inline size_t heap_caps_get_largest_free_block(uint32_t) { return 0; }
