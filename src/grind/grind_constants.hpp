#pragma once

/**
 * Constants used by GrindController.
 * Kept separate from Menu.hpp so the grind logic can be compiled and tested
 * on the native platform without pulling in Arduino/ESP32-specific headers.
 */

#define COFFEE_DOSE_OFFSET -2.5
#define MAX_GRINDING_TIME 20000UL           // ms — abort if grinding takes longer
#define GRINDING_FAILED_WEIGHT_TO_RESET 150 // g — weight to detect cup pressed on scale
