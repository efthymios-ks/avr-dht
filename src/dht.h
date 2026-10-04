#ifndef DHT_H
#define DHT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "dht_config.h"

// DHT11 / AM2302 (DHT22) temperature and humidity sensor driver for AVR.
//
// Integer-only API: temperature and humidity are reported scaled by 10
// (e.g. 251 == 25.1 C, 457 == 45.7 %RH). No floating-point library is
// pulled onto the target.
//
// All fail-able calls return dht_status. Results are written through the
// caller-supplied pointer; there is no global status getter.

typedef enum
{
	DHT_OK = 0,
	DHT_ERROR_TIMEOUT,
	DHT_ERROR_CHECKSUM,
	DHT_ERROR_OUT_OF_RANGE
} dht_status;

typedef struct
{
	// Temperature in tenths of a degree Celsius.
	//   DHT11: -200..600 (-20.0 .. 60.0 C, datasheet V1.3)
	//   DHT22: -400..800 (-40.0 .. 80.0 C)
	int16_t  temperature_celsius_x10;

	// Relative humidity in tenths of a percent.
	//   DHT11:  50..950   (5.0 .. 95.0 %RH, datasheet V1.3)
	//   DHT22:   0..1000  (0.0 .. 100.0 %RH)
	uint16_t humidity_percent_x10;
} dht_reading;

// Configure the data pin and wait DHT_INIT_DELAY_MS for the sensor to settle.
// Returns DHT_OK on success.
dht_status dht_init(void);

// Trigger a full read cycle and decode the result.
// Returns:
//   DHT_OK
//   DHT_ERROR_TIMEOUT       — no response or a bit window never ended.
//   DHT_ERROR_CHECKSUM      — the five-byte frame's checksum did not match.
//   DHT_ERROR_OUT_OF_RANGE  — decoded reading is outside the datasheet range.
dht_status dht_read(dht_reading *reading);

// Trigger a read and return the raw 5-byte frame exactly as received,
// including the checksum byte at frame[4]. The checksum is still verified;
// on checksum failure the frame is still copied out.
dht_status dht_read_raw(uint8_t frame[5]);

// Decode a 5-byte frame into a dht_reading. Exposed for host-side tests;
// does not touch any I/O. Verifies checksum and range per DHT_TYPE.
dht_status dht_decode_frame(const uint8_t frame[5], dht_reading *reading);

#ifdef __cplusplus
}
#endif

#endif
