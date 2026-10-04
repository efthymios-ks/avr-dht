#include "avr/io.h"

uint8_t PORTB, DDRB, PINB;
uint8_t PORTC, DDRC, PINC;
uint8_t PORTD, DDRD, PIND;

uint8_t TWBR, TWCR, TWSR, TWDR, TWAR;

void fake_io_reset(void)
{
    PORTB = DDRB = PINB = 0;
    PORTC = DDRC = PINC = 0;
    PORTD = DDRD = PIND = 0;
    TWBR = TWCR = TWSR = TWDR = TWAR = 0;
}
