#pragma once

/**
 * Constants used by GrindController.
 * Kept separate from Menu.hpp so the grind logic can be compiled and tested
 * on the native platform without pulling in Arduino/ESP32-specific headers.
 */

#define COFFEE_DOSE_OFFSET -2.5             // default fallback offset (legacy)
#define MAX_GRINDING_TIME 20000UL           // ms — abort if grinding takes longer
#define GRINDING_FAILED_WEIGHT_TO_RESET 150 // g — weight to detect cup pressed on scale
#define TARE_WAIT_MS 1400                   // ms — wait after tare before grinding starts

// Predictive grind-by-weight algorithm
#define GRIND_MOTOR_LATENCY_MS 250.0        // estimated motor coast time (ms); tune per grinder
#define GRIND_FLOW_WINDOW_MS 1500           // flow rate measurement window (ms)
#define GRIND_FLOW_THRESHOLD_GPS 0.3        // min g/s to consider flow as valid
#define GRIND_SETTLING_MS 1000              // wait after motor stop for scale to settle (ms)
#define GRIND_PULSE_TOLERANCE_G 0.3         // acceptable undershoot — no pulse needed below this (g)
#define GRIND_MAX_PULSE_ATTEMPTS 4          // max correction pulses before declaring done
#define GRIND_PULSE_MAX_DURATION_MS 600.0   // cap on correction pulse duration (ms)
#define GRIND_PULSE_FLOW_FALLBACK_GPS 1.5   // fallback flow rate when none was measured (g/s)
#define GRIND_STALL_WINDOW_MS 4000          // stall detection: no weight change in this window → fail
