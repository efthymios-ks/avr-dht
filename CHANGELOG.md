# Changelog

## v2.0.0 — 2026-10-04

Complete rewrite. **All public symbols are renamed** — this release is a hard break with v1.  
Portfolio project, no backward-compatibility shims.  
See the migration plan §8.4 for the full rename table.

### Renames

| v1                                            | v2                                                    |
|-----------------------------------------------|-------------------------------------------------------|
| `DHT_Setup()`                                 | `dht_init()`                                          |
| `DHT_Read(double *T, double *H)`              | `dht_read(dht_reading *reading)` (integer-only)       |
| `DHT_GetTemperature()`, `DHT_GetHumidity()`   | **removed** — use `dht_read()`                        |
| `DHT_ReadRaw(uint8_t data[4])`                | `dht_read_raw(uint8_t frame[5])` (full 5-byte frame)  |
| `DHT_GetStatus()`                             | **removed** — status is the function's return value   |
| `DHT_CelsiusToFahrenheit`, `DHT_CelsiusToKelvin` | **removed** — one line of user code (see README)    |
| `enum DHT_Status_t { DHT_Ok, ... }`           | `typedef enum { DHT_OK, DHT_ERROR_TIMEOUT, DHT_ERROR_CHECKSUM, DHT_ERROR_OUT_OF_RANGE } dht_status` |
| `DHT_Type`, `DHT_Pin`                         | `DHT_TYPE`, `DHT_PIN`                                 |
| `DHT11`, `DHT22`                              | `DHT_TYPE_DHT11`, `DHT_TYPE_DHT22`                    |

### Fixes

- **Build failure.** The old `Demo.c` called `DHT_Status()` (no such function —
  it was `DHT_GetStatus()`), so the shipped example did not compile. Rewritten
  against the new API with a working UART loop.
- **NULL pointer crash.** `DHT_GetTemperature()` and `DHT_GetHumidity()` were
  implemented by calling `DHT_Read(ptr, NULL)` / `DHT_Read(NULL, ptr)`, which
  then dereferenced the NULL pointer and wrote through it (on AVR this
  silently stores to register r0). Fixed by removing both wrappers; the new
  single-struct API cannot be misused this way, and all pointers are checked.
- **Timeouts were longer than intended.** The old loops did `_delay_us(2)` plus
  loop overhead per iteration, which added up to tens of extra microseconds
  per phase. Timeouts are now expressed as a microsecond budget with a 1 µs
  quantum, so the effective window matches the datasheet number.
- **DHT11 decimal byte ignored.** V1.3 of the DHT11 datasheet reports a
  temperature decimal part in byte 3; the old code dropped it. Bit 7 of that
  byte is also the sign bit, so sub-zero readings came back positive. Both
  are now honored.
- **DHT11 range wrong.** The old code validated against 0..50 °C and
  20..90 %RH; datasheet V1.3 gives −20..60 °C and 5..95 %RH. Updated.
- **Start-pulse timing.** The host-low pulse was 50 ms for DHT11 and 20 ms for
  DHT22, both outside the recommended typical. Fixed to 20 ms (DHT11) and
  2 ms (DHT22), which falls inside the 0.8–20 ms window for AM2302.
- **Power-on wait is now per-model.** DHT11 needs 1 s to stabilize, AM2302
  needs 2 s. The old code always waited 2 s. Now overridable via the
  `DHT_INIT_DELAY_MS` config macro; set to 0 to skip entirely.
- **README image link.** Pointed at the old repo name `AVR-DHT-Library`;
  updated to the current repo and moved under `docs/images/`.

### Features and optimization

- Integer-only API (`int16_t temperature_celsius_x10`,
  `uint16_t humidity_percent_x10`). Dropping `double` removes the `libm`
  dependency and the pull-in of ~1.5 kB of soft-float on small AVRs.
- Four nearly-identical polling loops collapsed into a single `static` helper
  (`wait_while_level`).
- Optional `DHT_ATOMIC` config wraps the time-critical bit-reading phase in
  `ATOMIC_BLOCK(ATOMIC_RESTORESTATE)` so a user ISR cannot stretch the bit
  windows.
- Separate internal decoders for DHT11 and DHT22 (`dht_decode_frame_dht11`,
  `dht_decode_frame_dht22`), declared in `src/dht_internal.h` so host-side
  tests can exercise both without recompiling the library twice.

### Project changes

- Repo renamed `AVR-DHT` → `avr-dht`.
- Restructured to `src/ examples/ tests/ sim/ docs/ scripts/` (see migration
  plan §4).
- Added `Build.ps1`, `Simulate.ps1`, `scripts/Common.psm1` for Windows + Linux
  build/test automation. Shared byte-identical with sibling repos.
- Added host-side Unity-style unit tests under `tests/` with fake AVR
  registers; covers both DHT11 and DHT22 frame layouts, sign-bit handling,
  checksum, range validation, and NULL guards.
- Added GitHub Actions CI running the matrix build + tests on Ubuntu.
- Added README template (features, wiring table, quick start, API table,
  memory usage, build steps).
- Datasheets moved to `docs/datasheets/` and renamed lowercase
  (`dht11.pdf`, `am2302-dht22.pdf`).
- Types no longer use the `_t` suffix (`dht_status` not `dht_status_t`);
  comments use `//` form in sentence case.

## v1 — initial release

Original Arduino-style API (`DHT_Setup`, `DHT_Read(double*, double*)`, `DHT_GetStatus`).
