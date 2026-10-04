# avr-dht

DHT11 / AM2302 (DHT22) temperature and humidity sensor driver for AVR microcontrollers.

[![ci](https://github.com/efthymios-ks/avr-dht/actions/workflows/ci.yml/badge.svg)](https://github.com/efthymios-ks/avr-dht/actions/workflows/ci.yml)

## Features
- Supports both DHT11 (datasheet V1.3) and AM2302 / DHT22.
- Pure bit-bang; no hardware timer, no interrupt handler.
- Optional `ATOMIC_BLOCK` wrapper around the bit-reading phase so interrupts cannot stretch the 26-70 us bit windows.
- Status is the function's return value — no global getter, no hidden state.
- Readings reported as scaled integers (`temperature_celsius_x10`, `humidity_percent_x10`), so the compiler never has to link libm.
- Host-side Unity tests cover the frame decoder for both sensor types.

## Supported MCUs and toolchain
Any AVR with `PORTx` / `DDRx` / `PINx` registers.  
Verified on ATmega328P (default, DIP-28) and ATmega32.  
Toolchain: avr-gcc with `-std=gnu99`.  
Default `F_CPU` is 8 MHz; the datasheet windows are generous enough that anything from 1 MHz upwards works unmodified.

## Wiring (default demo, ATmega328P DIP-28)

| Signal   | Device pin    | AVR pin | DIP-28 | Config                 | Description |
|----------|---------------|---------|--------|------------------------|-------------|
| DATA     | DHT22 pin 2   | PD2     | 4      | `#define DHT_PIN D, 2` | Bidirectional data line; 4.7 kOhm pull-up to VDD (5.1 kOhm for AM2302 on long cables) |
| VDD      | DHT22 pin 1   | VCC     | 7      | —                      | 3.3-5.5 V |
| GND      | DHT22 pin 4   | GND     | 8      | —                      | Common ground |
| UART TX  | SerialTerm RX | PD1     | 3      | —                      | Demo prints readings at 9600 baud, 8N1 |

DHT22 pin 3 (NC) is not connected. If you are using a 3-pin breakout board, the pull-up resistor is already on the board.

## Quick start

```c
#include <util/delay.h>
#include "dht.h"

int main(void)
{
    // Idle the data pin and wait for the sensor to settle.
    dht_init();

    dht_reading reading;
    while (1) {
        // Trigger a read; status carries any timeout, checksum, or range error.
        dht_status status = dht_read(&reading);
        if (status == DHT_OK) {
            // reading.temperature_celsius_x10 == 257 means 25.7 C.
            // reading.humidity_percent_x10    == 658 means 65.8 %.
        }

        // DHT22: minimum 2 s between reads. DHT11: minimum 1 s.
        for (uint8_t i = 0; i < 20; i++) {
            _delay_ms(100);
        }
    }
}
```

Need Fahrenheit or Kelvin? One line each.

```c
int16_t fahrenheit_x10 = (int16_t)((int32_t)reading.temperature_celsius_x10 * 9 / 5 + 320);
int16_t kelvin_x10     = (int16_t)(reading.temperature_celsius_x10 + 2732);
```

## API

### dht_init
- `dht_status dht_init(void)`
- Does: configures the data pin and waits `DHT_INIT_DELAY_MS` for the sensor to settle.
- Returns: `DHT_OK` on success.
- Notes: call once before `dht_read` or `dht_read_raw`.

### dht_read
- `dht_status dht_read(dht_reading *reading)`
- Does: triggers a full read cycle, decodes the frame, and verifies the datasheet range.
- Returns: `DHT_OK`, `DHT_ERROR_TIMEOUT` (no response or a bit window never ended), `DHT_ERROR_CHECKSUM`, or `DHT_ERROR_OUT_OF_RANGE`.
- Params: `reading` — output struct; must be non-null.
- Notes: blocks for up to ~25 ms. If `DHT_ATOMIC = 1`, interrupts are disabled for the ~5 ms bit-read phase.

### dht_read_raw
- `dht_status dht_read_raw(uint8_t frame[5])`
- Does: triggers a read and copies out the raw 5-byte frame exactly as received, including the checksum at `frame[4]`.
- Returns: `DHT_OK`, `DHT_ERROR_TIMEOUT`, or `DHT_ERROR_CHECKSUM`.
- Notes: on checksum failure the frame is still copied out so callers can inspect it.

### dht_decode_frame
- `dht_status dht_decode_frame(const uint8_t frame[5], dht_reading *reading)`
- Does: decodes a 5-byte frame into a `dht_reading` using the decoder matching the compiled `DHT_TYPE`.
- Returns: `DHT_OK`, `DHT_ERROR_CHECKSUM`, or `DHT_ERROR_OUT_OF_RANGE`.
- Notes: pure function; does no I/O. Exposed so host-side tests can exercise the decoder without hardware.

### dht_status
- `DHT_OK` — success.
- `DHT_ERROR_TIMEOUT` — sensor did not drive a required edge within the datasheet window.
- `DHT_ERROR_CHECKSUM` — frame's checksum byte did not match the sum of bytes 0..3.
- `DHT_ERROR_OUT_OF_RANGE` — decoded value is outside the datasheet range, or a null pointer was passed.

### Sensor type constants
- `DHT_TYPE_DHT11` — selects the DHT11 decoder.
- `DHT_TYPE_DHT22` — selects the AM2302 / DHT22 decoder.

## Configuration

### DHT_TYPE
- `#define DHT_TYPE DHT_TYPE_DHT22`
- Does: picks the sensor model; must be `DHT_TYPE_DHT11` or `DHT_TYPE_DHT22`.
- Notes: a compile-time check rejects any other value.

### DHT_PIN
- `#define DHT_PIN D, 2`
- Does: data-pin port letter and bit, written as two tokens so `io_macros.h` can paste them into `PORTx` / `DDRx` / `PINx`.

### DHT_ATOMIC
- `#define DHT_ATOMIC 1`
- Does: when `1`, wraps the bit-reading phase in `ATOMIC_BLOCK(ATOMIC_RESTORESTATE)` so interrupts cannot stretch the 26-70 us bit windows.
- Notes: set to `0` only if your application has no interrupts enabled during reads.

### DHT_INIT_DELAY_MS
- `#define DHT_INIT_DELAY_MS 1000` (DHT11) or `2000` (DHT22)
- Does: power-on stabilisation delay used by `dht_init`, in milliseconds.
- Notes: set to `0` to skip (for example when your application already delayed elsewhere).

## Memory usage
Build.ps1 writes `build/size.txt`;  
Before/after comparison on ATmega32 (v1's historical target).

| Build | Flash / RAM |
|-------|------------:|
| v1 (ATmega32, -Os) | did not build |
| v2 (ATmega32, -Os) | 2698 B / 0 B |

## Build, test, simulate

```powershell
.\Build.ps1
.\Build.ps1 -AllMcus -DebugBuild
.\Build.ps1 -Test
.\Simulate.ps1
.\Simulate.ps1 -NoLaunch
```

Build.ps1 and Simulate.ps1 install the AVR toolchain (and host gcc for `-Test`, SimulIDE for `.\Simulate.ps1`) into a shared per-user cache folder on first run — no admin, no system-wide PATH changes.  
Pass `-RemoveTools` to uninstall what the scripts installed when the run ends.  
Pass `-NoInstall` to fail loudly instead if the tools are missing.

## Limitations
- Host-side tests cover the frame decoder and checksum / range validation only. The bit-timing code depends on `_delay_us`, which is a no-op on the host, so it cannot be exercised without real hardware or a cycle-accurate simulator — see `sim/README.md` for the SimulIDE setup.
- The library is bit-banged. On an 8 MHz AVR, one `dht_read` blocks for up to ~25 ms (start pulse) plus up to ~5 ms (40 bits). If `DHT_ATOMIC = 1`, interrupts are disabled for the bit-read portion (~5 ms worst case).

## Changelog and license
See [CHANGELOG.md](CHANGELOG.md).  
MIT — see [LICENSE](LICENSE).
