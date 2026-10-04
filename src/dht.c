#include "dht.h"
#include "dht_internal.h"

#include <stddef.h>

#include <avr/io.h>
#include <util/delay.h>

#if DHT_ATOMIC && defined(__AVR__)
#	include <util/atomic.h>
#	define DHT_ATOMIC_ENABLED 1
#else
#	define DHT_ATOMIC_ENABLED 0
#endif

#include "io_macros.h"

// Datasheet constants (DHT11 V1.3 / AM2302).
//
// Host start pulse:
//   DHT11:  low >= 18 ms (typ 20 ms), then release.
//   DHT22:  low 0.8 .. 20 ms (typ 1 ms), then release.
//
// Sensor response:
//   ~20-40 us low, 80 us low (ready), 80 us high (ready), then 40 bits.
//
// Bit encoding:
//   50 us low lead-in, then 26-28 us high == '0', 70 us high == '1'.
//
// We take the host-release timing per model, but use the same wide timeout
// (~200 us) for every response-phase edge so an odd sensor is not falsely
// flagged.

#if (DHT_TYPE == DHT_TYPE_DHT11)
#	define DHT_START_LOW_MS        20u
#else // DHT_TYPE_DHT22
#	define DHT_START_LOW_MS        2u
#endif

// Microsecond-budget timeouts. One iteration of wait_while_level() is
// `_delay_us(1)` plus a handful of cycles of loop overhead — roughly 2 us
// on an 8 MHz AVR. Giving each phase a count equal to the budget in us
// leaves 2x headroom, which is what we want for the generous datasheet
// windows.
#define DHT_RESPONSE_TIMEOUT_US   200u   // Each of the three sensor phases.
#define DHT_BIT_LOW_TIMEOUT_US    100u   // 50 us leading-low per bit.
#define DHT_BIT_HIGH_TIMEOUT_US   100u   // Up to 70 us high per bit.

// Poll the data pin while it stays at `level`. Returns 0 on success,
// 1 on timeout. One iteration takes approximately 2 us at 8 MHz, so we
// sleep 1 us per iteration and allow `timeout_us` iterations.
static uint8_t wait_while_level(uint8_t level, uint16_t timeout_us)
{
	while (((IO_READ(DHT_PIN) ? 1u : 0u)) == level)
	{
		if (timeout_us == 0u) { return 1u; }
		_delay_us(1);
		timeout_us--;
	}
	return 0u;
}

static void send_start_pulse(void)
{
	// Drive the line low for the model-specific start-pulse width.
	IO_WRITE(DHT_PIN, IO_LOW);
	IO_MODE(DHT_PIN,  IO_OUTPUT);

	for (uint8_t i = 0; i < DHT_START_LOW_MS; i++)
	{
		_delay_ms(1);
	}

	// Release the bus: enable pull-up and switch to input.
	IO_WRITE(DHT_PIN, IO_HIGH);
	IO_MODE(DHT_PIN,  IO_INPUT);
}

// Wait out the three edges of the sensor's response (20-40 us high,
// 80 us low, 80 us high).
static dht_status wait_for_response(void)
{
	// 1. Line is still high from the pull-up — sensor pulls low within 20-40 us.
	if (wait_while_level(1u, DHT_RESPONSE_TIMEOUT_US)) { return DHT_ERROR_TIMEOUT; }
	// 2. 80 us low from sensor.
	if (wait_while_level(0u, DHT_RESPONSE_TIMEOUT_US)) { return DHT_ERROR_TIMEOUT; }
	// 3. 80 us high from sensor.
	if (wait_while_level(1u, DHT_RESPONSE_TIMEOUT_US)) { return DHT_ERROR_TIMEOUT; }
	return DHT_OK;
}

// Clock in 40 bits (5 bytes, MSB first). Each bit is a ~50 us low lead-in
// followed by a 26-28 us ('0') or 70 us ('1') high period.
static dht_status read_frame(uint8_t frame[5])
{
	for (uint8_t byte_index = 0; byte_index < 5u; byte_index++)
	{
		uint8_t value = 0u;
		for (int8_t bit_index = 7; bit_index >= 0; bit_index--)
		{
			// 50 us low lead-in.
			if (wait_while_level(0u, DHT_BIT_LOW_TIMEOUT_US))
			{
				return DHT_ERROR_TIMEOUT;
			}

			// After 35 us, '0' has returned low and '1' is still high.
			_delay_us(35);
			if (IO_READ(DHT_PIN))
			{
				value |= (uint8_t)(1u << bit_index);

				// Wait out the rest of the '1' high period.
				if (wait_while_level(1u, DHT_BIT_HIGH_TIMEOUT_US))
				{
					return DHT_ERROR_TIMEOUT;
				}
			}
		}
		frame[byte_index] = value;
	}
	return DHT_OK;
}

// Public API.

dht_status dht_init(void)
{
	// Idle state: input, pull-up enabled.
	IO_WRITE(DHT_PIN, IO_HIGH);
	IO_MODE(DHT_PIN,  IO_INPUT);

#if (DHT_INIT_DELAY_MS > 0u)
	for (uint16_t i = 0; i < (uint16_t)DHT_INIT_DELAY_MS; i++)
	{
		_delay_ms(1);
	}
#endif

	return DHT_OK;
}

dht_status dht_read_raw(uint8_t frame[5])
{
	dht_status status;

	if (frame == NULL) { return DHT_ERROR_OUT_OF_RANGE; }

	frame[0] = frame[1] = frame[2] = frame[3] = frame[4] = 0u;

	send_start_pulse();

#if DHT_ATOMIC_ENABLED
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		status = wait_for_response();
		if (status == DHT_OK)
		{
			status = read_frame(frame);
		}
	}
#else
	status = wait_for_response();
	if (status == DHT_OK)
	{
		status = read_frame(frame);
	}
#endif

	if (status != DHT_OK) { return status; }

	// Checksum = low byte of sum(frame[0..3]).
	if ((uint8_t)(frame[0] + frame[1] + frame[2] + frame[3]) != frame[4])
	{
		return DHT_ERROR_CHECKSUM;
	}

	return DHT_OK;
}

// DHT11 V1.3 frame layout:
//   byte 0 = humidity integer part (%).
//   byte 1 = humidity decimal part (always 0 in V1.2, used in V1.3).
//   byte 2 = temperature integer part (C).
//   byte 3 = temperature decimal part; bit 7 marks a negative temperature.
//   byte 4 = checksum (sum of bytes 0..3, low byte).
dht_status dht_decode_frame_dht11(const uint8_t frame[5], dht_reading *reading)
{
	if (frame == NULL || reading == NULL) { return DHT_ERROR_OUT_OF_RANGE; }

	if ((uint8_t)(frame[0] + frame[1] + frame[2] + frame[3]) != frame[4])
	{
		return DHT_ERROR_CHECKSUM;
	}

	uint16_t humidity_x10 = (uint16_t)frame[0] * 10u + (uint16_t)(frame[1] & 0x0Fu);

	int16_t temperature_x10 = (int16_t)((int16_t)frame[2] * 10 + (int16_t)(frame[3] & 0x7Fu));
	if (frame[3] & 0x80u) { temperature_x10 = (int16_t)(-temperature_x10); }

	reading->humidity_percent_x10    = humidity_x10;
	reading->temperature_celsius_x10 = temperature_x10;

	if (temperature_x10 < -200 || temperature_x10 > 600) { return DHT_ERROR_OUT_OF_RANGE; }
	if (humidity_x10 < 50u || humidity_x10 > 950u)       { return DHT_ERROR_OUT_OF_RANGE; }

	return DHT_OK;
}

// AM2302/DHT22 frame layout:
//   bytes 0..1 = humidity * 10, big-endian (0..1000 == 0.0..100.0 %RH).
//   bytes 2..3 = temperature * 10, big-endian; bit 15 of the word is the sign.
//   byte 4     = checksum.
dht_status dht_decode_frame_dht22(const uint8_t frame[5], dht_reading *reading)
{
	if (frame == NULL || reading == NULL) { return DHT_ERROR_OUT_OF_RANGE; }

	if ((uint8_t)(frame[0] + frame[1] + frame[2] + frame[3]) != frame[4])
	{
		return DHT_ERROR_CHECKSUM;
	}

	uint16_t humidity_x10 = (uint16_t)(((uint16_t)frame[0] << 8) | (uint16_t)frame[1]);

	uint16_t raw_temperature = (uint16_t)(((uint16_t)(frame[2] & 0x7Fu) << 8) | (uint16_t)frame[3]);
	int16_t  temperature_x10 = (int16_t)raw_temperature;
	if (frame[2] & 0x80u) { temperature_x10 = (int16_t)(-temperature_x10); }

	reading->humidity_percent_x10    = humidity_x10;
	reading->temperature_celsius_x10 = temperature_x10;

	if (temperature_x10 < -400 || temperature_x10 > 800) { return DHT_ERROR_OUT_OF_RANGE; }
	if (humidity_x10 > 1000u)                            { return DHT_ERROR_OUT_OF_RANGE; }

	return DHT_OK;
}

dht_status dht_decode_frame(const uint8_t frame[5], dht_reading *reading)
{
#if (DHT_TYPE == DHT_TYPE_DHT11)
	return dht_decode_frame_dht11(frame, reading);
#else
	return dht_decode_frame_dht22(frame, reading);
#endif
}

dht_status dht_read(dht_reading *reading)
{
	uint8_t frame[5];
	dht_status status;

	if (reading == NULL) { return DHT_ERROR_OUT_OF_RANGE; }

	status = dht_read_raw(frame);
	if (status != DHT_OK) { return status; }

	return dht_decode_frame(frame, reading);
}
