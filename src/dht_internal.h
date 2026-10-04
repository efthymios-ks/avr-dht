// Internal API for dht. Not for application code — subject to change without notice.
// Included only by src/dht.c and tests/test_dht.c.

#ifndef DHT_INTERNAL_H
#define DHT_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "dht.h"

// Decode a DHT11 V1.3 frame. Verifies checksum and clamps to the datasheet
// range (-20.0 .. 60.0 C, 5.0 .. 95.0 %RH).
dht_status dht_decode_frame_dht11(const uint8_t frame[5], dht_reading *reading);

// Decode an AM2302/DHT22 frame. Verifies checksum and clamps to the datasheet
// range (-40.0 .. 80.0 C, 0.0 .. 100.0 %RH).
dht_status dht_decode_frame_dht22(const uint8_t frame[5], dht_reading *reading);

#ifdef __cplusplus
}
#endif

#endif
