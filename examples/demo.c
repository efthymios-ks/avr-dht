// demo.c — read a DHT22 (default) every ~2 s and emit the result over the
// ATmega328P hardware UART at 9600 baud. See sim/README.md for the SimulIDE
// setup.

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>

#include "dht.h"

#ifndef F_CPU
#	error "F_CPU must be defined (use -DF_CPU=8000000UL)."
#endif

#define UART_BAUD        9600UL
#define UART_UBRR_VALUE  ((F_CPU / (16UL * UART_BAUD)) - 1UL)

#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__) || defined(__AVR_ATmega48__)
#	define UART_UBRRH   UBRR0H
#	define UART_UBRRL   UBRR0L
#	define UART_UCSRA   UCSR0A
#	define UART_UCSRB   UCSR0B
#	define UART_UCSRC   UCSR0C
#	define UART_UDR     UDR0
#	define UART_UDRE    UDRE0
#	define UART_TXEN    TXEN0
#	define UART_UCSZ0   UCSZ00
#	define UART_UCSZ1   UCSZ01
#elif defined(__AVR_ATmega32__) || defined(__AVR_ATmega16__)
#	define UART_UBRRH   UBRRH
#	define UART_UBRRL   UBRRL
#	define UART_UCSRA   UCSRA
#	define UART_UCSRB   UCSRB
#	define UART_UCSRC   UCSRC
#	define UART_UDR     UDR
#	define UART_UDRE    UDRE
#	define UART_TXEN    TXEN
#	define UART_UCSZ0   UCSZ0
#	define UART_UCSZ1   UCSZ1
#else
#	error "Unsupported MCU for the demo UART. Add register aliases above."
#endif

static void uart_init(void)
{
	UART_UBRRH = (uint8_t)(UART_UBRR_VALUE >> 8);
	UART_UBRRL = (uint8_t) UART_UBRR_VALUE;
	UART_UCSRB = (1 << UART_TXEN);
#if defined(__AVR_ATmega32__) || defined(__AVR_ATmega16__)
	// ATmega32 UCSRC requires URSEL bit set to write it.
	UART_UCSRC = (1 << URSEL) | (1 << UART_UCSZ1) | (1 << UART_UCSZ0);
#else
	UART_UCSRC = (1 << UART_UCSZ1) | (1 << UART_UCSZ0);
#endif
}

static void uart_write_char(char c)
{
	while (!(UART_UCSRA & (1 << UART_UDRE))) { }
	UART_UDR = (uint8_t)c;
}

static void uart_write(const char *s)
{
	while (*s) { uart_write_char(*s++); }
}

static void uart_write_signed_x10(int16_t value_x10)
{
	char buffer[12];
	int16_t integer_part;
	uint16_t fraction_part;

	if (value_x10 < 0)
	{
		uart_write_char('-');
		value_x10 = (int16_t)(-value_x10);
	}
	integer_part  = (int16_t)(value_x10 / 10);
	fraction_part = (uint16_t)(value_x10 % 10);
	snprintf(buffer, sizeof(buffer), "%d.%u", integer_part, fraction_part);
	uart_write(buffer);
}

static void uart_write_unsigned_x10(uint16_t value_x10)
{
	char buffer[12];
	snprintf(buffer, sizeof(buffer), "%u.%u",
	         (unsigned)(value_x10 / 10u),
	         (unsigned)(value_x10 % 10u));
	uart_write(buffer);
}

int main(void)
{
	dht_reading reading;
	dht_status  status;

	uart_init();
	dht_init();

	uart_write("avr-dht demo\r\n");

	while (1)
	{
		status = dht_read(&reading);
		switch (status)
		{
			case DHT_OK:
				uart_write("T=");
				uart_write_signed_x10(reading.temperature_celsius_x10);
				uart_write(" C  H=");
				uart_write_unsigned_x10(reading.humidity_percent_x10);
				uart_write(" %\r\n");
				break;
			case DHT_ERROR_TIMEOUT:
				uart_write("err: timeout\r\n");
				break;
			case DHT_ERROR_CHECKSUM:
				uart_write("err: checksum\r\n");
				break;
			case DHT_ERROR_OUT_OF_RANGE:
				uart_write("err: out of range\r\n");
				break;
		}

		// Datasheet: DHT22 can be sampled every 2 s, DHT11 every 1 s.
		for (uint8_t i = 0; i < 20; i++) { _delay_ms(100); }
	}
}
