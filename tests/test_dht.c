#include "unity.h"
#include "dht.h"
#include "dht_internal.h"

// Pure-logic tests for the DHT frame decoder. The I/O path uses _delay_us
// (a no-op on the host) so it cannot be verified without hardware or a
// cycle-accurate simulator; see sim/README.md.

static uint8_t with_checksum(uint8_t frame[5])
{
	frame[4] = (uint8_t)(frame[0] + frame[1] + frame[2] + frame[3]);
	return frame[4];
}

// DHT11.

static void dht11_positive_integer_value_decodes(void)
{
	uint8_t frame[5] = { 45u, 0u, 25u, 0u, 0u };
	with_checksum(frame);

	dht_reading reading = {0, 0};
	TEST_ASSERT_EQUAL_INT(DHT_OK, dht_decode_frame_dht11(frame, &reading));
	TEST_ASSERT_EQUAL_UINT16(450u, reading.humidity_percent_x10);     // 45.0 %.
	TEST_ASSERT_EQUAL_INT16(250, reading.temperature_celsius_x10);     // 25.0 C.
}

static void dht11_negative_value_uses_sign_bit_of_decimal_byte(void)
{
	// 10.1 C with bit 7 of the decimal byte set means -10.1 C.
	uint8_t frame[5] = { 30u, 0u, 10u, 0x81u, 0u };
	with_checksum(frame);

	dht_reading reading = {0, 0};
	TEST_ASSERT_EQUAL_INT(DHT_OK, dht_decode_frame_dht11(frame, &reading));
	TEST_ASSERT_EQUAL_INT16(-101, reading.temperature_celsius_x10);
	TEST_ASSERT_EQUAL_UINT16(300u, reading.humidity_percent_x10);
}

static void dht11_reading_out_of_range_is_rejected(void)
{
	// 70 C is above the datasheet V1.3 max of 60 C.
	uint8_t frame[5] = { 30u, 0u, 70u, 0u, 0u };
	with_checksum(frame);

	dht_reading reading = {0, 0};
	TEST_ASSERT_EQUAL_INT(DHT_ERROR_OUT_OF_RANGE, dht_decode_frame_dht11(frame, &reading));
}

// DHT22.

static void dht22_positive_value_decodes(void)
{
	// Humidity 658 (65.8 %), temperature 257 (25.7 C).
	uint8_t frame[5] = { 0x02u, 0x92u, 0x01u, 0x01u, 0u };
	with_checksum(frame);

	dht_reading reading = {0, 0};
	TEST_ASSERT_EQUAL_INT(DHT_OK, dht_decode_frame_dht22(frame, &reading));
	TEST_ASSERT_EQUAL_UINT16(658u, reading.humidity_percent_x10);
	TEST_ASSERT_EQUAL_INT16(257, reading.temperature_celsius_x10);
}

static void dht22_negative_value_uses_sign_bit_of_high_byte(void)
{
	// Humidity 500 (50.0 %), temperature 0x8019 means -(0x0019) = -25 (-2.5 C).
	uint8_t frame[5] = { 0x01u, 0xF4u, 0x80u, 0x19u, 0u };
	with_checksum(frame);

	dht_reading reading = {0, 0};
	TEST_ASSERT_EQUAL_INT(DHT_OK, dht_decode_frame_dht22(frame, &reading));
	TEST_ASSERT_EQUAL_INT16(-25, reading.temperature_celsius_x10);
	TEST_ASSERT_EQUAL_UINT16(500u, reading.humidity_percent_x10);
}

// Checksum.

static void bad_checksum_is_rejected_for_both_types(void)
{
	uint8_t frame[5] = { 10u, 20u, 30u, 40u, 0u };
	with_checksum(frame);
	frame[4] ^= 0x01u;   // Corrupt.

	dht_reading reading = {0, 0};
	TEST_ASSERT_EQUAL_INT(DHT_ERROR_CHECKSUM, dht_decode_frame_dht11(frame, &reading));
	TEST_ASSERT_EQUAL_INT(DHT_ERROR_CHECKSUM, dht_decode_frame_dht22(frame, &reading));
}

// NULL handling.

static void null_output_pointer_is_rejected(void)
{
	uint8_t frame[5] = { 45u, 0u, 25u, 0u, 70u };
	TEST_ASSERT_EQUAL_INT(DHT_ERROR_OUT_OF_RANGE, dht_decode_frame_dht11(frame, NULL));
	TEST_ASSERT_EQUAL_INT(DHT_ERROR_OUT_OF_RANGE, dht_decode_frame_dht22(frame, NULL));
}

void setUp(void)    { }
void tearDown(void) { }

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(dht11_positive_integer_value_decodes);
	RUN_TEST(dht11_negative_value_uses_sign_bit_of_decimal_byte);
	RUN_TEST(dht11_reading_out_of_range_is_rejected);
	RUN_TEST(dht22_positive_value_decodes);
	RUN_TEST(dht22_negative_value_uses_sign_bit_of_high_byte);
	RUN_TEST(bad_checksum_is_rejected_for_both_types);
	RUN_TEST(null_output_pointer_is_rejected);
	return UNITY_END();
}
