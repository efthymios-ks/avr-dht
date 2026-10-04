#ifndef FAKE_AVR_IO_H
#define FAKE_AVR_IO_H

// Fake avr/io.h for host-side unit tests.
// Provides plain uint8_t storage for the PORTx/DDRx/PINx registers used by
// io_macros.h, so tests can read/write them and verify macro behavior.
// Also exposes the TWI registers (TWBR, TWCR, TWSR, TWDR, TWAR) and their bit
// positions so twi.c compiles unchanged on the host.

#include <stdint.h>

extern uint8_t PORTB, DDRB, PINB;
extern uint8_t PORTC, DDRC, PINC;
extern uint8_t PORTD, DDRD, PIND;

extern uint8_t TWBR, TWCR, TWSR, TWDR, TWAR;

// TWI control register bit positions. Values match the ATmega328P and
// ATmega32 datasheets.
#define TWINT 7
#define TWEA  6
#define TWSTA 5
#define TWSTO 4
#define TWWC  3
#define TWEN  2
#define TWIE  0

// TWI status register prescaler bits.
#define TWPS0 0
#define TWPS1 1

#ifndef _BV
#define _BV(bit) (1u << (bit))
#endif

// Reset every emulated register (I/O and TWI) to 0.
void fake_io_reset(void);

#endif
