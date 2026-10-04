#ifndef DHT_CONFIG_H
#define DHT_CONFIG_H

// Build-time configuration for the DHT driver.
//
// Every macro below is wrapped in `#ifndef`, so you can override it from the
// command line (`-DDHT_TYPE=DHT_TYPE_DHT11`) or by force-including a project
// config header via `-include my_config.h`.

#define DHT_TYPE_DHT11  0u
#define DHT_TYPE_DHT22  1u

#ifndef DHT_TYPE
#define DHT_TYPE   DHT_TYPE_DHT22
#endif

// Data line pin. Written as two tokens (port letter, bit) so io_macros.h can
// paste them into PORTx/DDRx/PINx at compile time.
#ifndef DHT_PIN
#define DHT_PIN    D, 2
#endif

// Set to 1 to wrap the bit-reading phase in ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
// so interrupts cannot stretch the 26-70 us bit windows. Set to 0 only if
// your application has no interrupts enabled during reads.
#ifndef DHT_ATOMIC
#define DHT_ATOMIC 1
#endif

// Power-on stabilisation delay in dht_init(), in milliseconds.
// DHT11 datasheet V1.3: 1 s. AM2302/DHT22 datasheet: 2 s.
// Set to 0 to skip (e.g. if your app already delayed elsewhere).
#ifndef DHT_INIT_DELAY_MS
#define DHT_INIT_DELAY_MS  ((DHT_TYPE == DHT_TYPE_DHT11) ? 1000u : 2000u)
#endif

#if (DHT_TYPE != DHT_TYPE_DHT11) && (DHT_TYPE != DHT_TYPE_DHT22)
#error "DHT_TYPE must be DHT_TYPE_DHT11 or DHT_TYPE_DHT22"
#endif

#endif
