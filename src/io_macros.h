#ifndef IO_MACROS_H
#define IO_MACROS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <avr/io.h>
#include <stdbool.h>
#include <stdint.h>

// Pin-oriented macros below take a pin written as two tokens: the port letter
// (B, C, D) and the bit number (0-7). Example:
//     #define LED B, 5
//     IO_MODE(LED, IO_OUTPUT);
//     IO_WRITE(LED, IO_HIGH);

#define IO_INPUT 0u
#define IO_OUTPUT 1u
#define IO_LOW 0u
#define IO_HIGH 1u

#define IO_MODE(pin, mode) ((mode) ? IO_SET_DDR_(pin) : IO_CLEAR_DDR_(pin))
#define IO_WRITE(pin, level) ((level) ? IO_SET_PORT_(pin) : IO_CLEAR_PORT_(pin))
#define IO_READ(pin) (IO_READ_PIN_(pin))
#define IO_TOGGLE(pin) (IO_TOGGLE_PORT_(pin))
#define IO_MODE_TOGGLE(pin) (IO_TOGGLE_DDR_(pin))

#define IO_BIT_SET(reg, bit) ((reg) |= (uint8_t)(1u << (bit)))
#define IO_BIT_CLEAR(reg, bit) ((reg) &= (uint8_t)~(1u << (bit)))
#define IO_BIT_TOGGLE(reg, bit) ((reg) ^= (uint8_t)(1u << (bit)))
#define IO_BIT_IS_SET(reg, bit) (((reg) & (uint8_t)(1u << (bit))) != 0u)

// Implementation details below. Trailing underscore marks them as not-for-direct-use.
#define IO_PORT_(letter) (PORT##letter)
#define IO_DDR_(letter) (DDR##letter)
#define IO_PIN_(letter) (PIN##letter)

#define IO_SET_DDR_(letter, bit) IO_BIT_SET(IO_DDR_(letter), (bit))
#define IO_CLEAR_DDR_(letter, bit) IO_BIT_CLEAR(IO_DDR_(letter), (bit))
#define IO_TOGGLE_DDR_(letter, bit) IO_BIT_TOGGLE(IO_DDR_(letter), (bit))
#define IO_SET_PORT_(letter, bit) IO_BIT_SET(IO_PORT_(letter), (bit))
#define IO_CLEAR_PORT_(letter, bit) IO_BIT_CLEAR(IO_PORT_(letter), (bit))
#define IO_TOGGLE_PORT_(letter, bit) IO_BIT_TOGGLE(IO_PORT_(letter), (bit))
#define IO_READ_PIN_(letter, bit) IO_BIT_IS_SET(IO_PIN_(letter), (bit))

#ifdef __cplusplus
}
#endif

#endif
