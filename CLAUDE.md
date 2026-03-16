# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**openGBW** is an embedded C++ project for an ESP32-based smart coffee grinder scale (grind-by-weight). It uses PlatformIO with the Arduino framework.

## Common Commands

```bash
# Build for ESP32
pio run -e esp32_usb

# Run unit tests (no hardware needed, uses native platform)
pio test -e native

# Run a single test suite
pio test -e native -f test_grind_controller
pio test -e native -f test_math_buffer

# Flash to ESP32
pio run -e esp32_usb --target upload

# Serial monitor
pio device monitor -b 115200
```

## Architecture

The project is split into hardware-dependent (ESP32 only) and hardware-independent (testable on native) code.

### Hardware-independent modules (testable on native)
- **`src/grind/GrindController`** — Core state machine for grind-by-weight logic. Takes dependency-injected callbacks for relay, tare, and offset saving. Maintains a `MathBuffer<double, 100>` ring buffer of timestamped weight readings and uses rolling windows to decide when to stop grinding. States: `STATUS_EMPTY → STATUS_GRINDING_IN_PROGRESS → STATUS_GRINDING_FINISHED` (and `STATUS_GRINDING_FAILED`).
- **`lib/MathBuffer/`** — Template ring buffer with time-windowed statistics (`averageSince()`, `maxSince()`, `minSince()`, `countSamplesSince()`). Used for smoothing weight history.

### Hardware-dependent modules (ESP32 only)
- **`src/scale`** — HX711 load cell reader with Kalman filtering. Runs as two FreeRTOS tasks: one reads raw HX711 data, the other updates global state/detects auto-tare/sleep. Exposes globals `scaleWeight`, `setWeight`, `offset`, `scaleReady`.
- **`src/display`** — OLED rendering via U8g2 (SSD1306 128x64). Runs as a FreeRTOS task.
- **`src/menu/`** — Hierarchical menu system driven by rotary encoder ISR. `MenuController` handles encoder debouncing and NVS persistence via the ESP32 `Preferences` library. `DeviceState` is a static state holder for `GrinderState` and `MenuId`.
- **`src/menu/pages/`** — One subdirectory per menu screen (8 screens: main, closed/home, offset, calibrate, grind mode, scale mode, sleep, purge, reset). All extend `Menu.hpp`.

### Data flow
```
HX711 → ScaleTask (Kalman filter) → GrindController (weight buffer + state machine)
                                          ↓ callbacks
                                    Relay / Tare / NVS save
RotaryEncoder ISR → MenuController → DeviceState
DisplayTask reads DeviceState + scaleWeight → OLED
```

## Testing

Tests live in `test/` and use the Unity framework via PlatformIO. Hardware stubs are in `test/stubs/` — mock implementations of ESP32-specific headers so `GrindController` and `MathBuffer` can build on native.

The `native` PlatformIO environment only builds the `src/grind/` and `lib/MathBuffer/` directories (configured via `build_src_filter` in `platformio.ini`).

## Pin Assignments

The README.md wiring diagram is outdated. For accurate GPIO pin assignments, read the source code directly (e.g., `src/main.cpp` and module headers).

## Key Constants

See `src/grind/grind_constants.hpp` for tunable grind parameters:
- Default offset: `-2.5g` (stops grinding 2.5g before target)
- Max grind time: `20000ms`
- Stall detection window: `4000ms` with no weight change → abort
