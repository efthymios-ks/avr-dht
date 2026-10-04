#ifndef FAKE_AVR_PGMSPACE_H
#define FAKE_AVR_PGMSPACE_H

#include <stdint.h>
#include <string.h>

// PROGMEM is a no-op on the host.
#define PROGMEM
#define PSTR(s) (s)

static inline uint8_t pgm_read_byte(const void *addr) { return *(const uint8_t *)addr; }
static inline uint16_t pgm_read_word(const void *addr) { return *(const uint16_t *)addr; }
static inline uint32_t pgm_read_dword(const void *addr) { return *(const uint32_t *)addr; }
static inline void *memcpy_P(void *dst, const void *src, size_t n) { return memcpy(dst, src, n); }
static inline size_t strlen_P(const char *s) { return strlen(s); }

#endif
