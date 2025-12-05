#pragma once

/**
 * @file Config.hpp
 * @brief Hardware configuration for Teensy 4.1 minimal example
 *
 * All hardware definitions are constexpr for zero runtime overhead.
 * Adjust pin numbers to match your wiring.
 *
 * Wiring reference:
 * - Encoders use quadrature signals (A/B pins)
 * - Buttons use internal pull-up (connect to GND when pressed)
 */

#include <array>

#include <drivers/arduino/input/ButtonController.hpp>
#include <drivers/common/EncoderDef.hpp>
#include <oc/hal/Types.hpp>

namespace Config {

// ═══════════════════════════════════════════════════════════════════
// Timing Configuration
// ═══════════════════════════════════════════════════════════════════

/// Long press threshold in milliseconds
constexpr uint32_t LONG_PRESS_MS = 500;

/// Double tap window in milliseconds
constexpr uint32_t DOUBLE_TAP_MS = 300;

/// Button debounce time in milliseconds
constexpr uint8_t DEBOUNCE_MS = 5;

// ═══════════════════════════════════════════════════════════════════
// Encoder Configuration
// ═════════════════════════════════════════════════════════════════

/**
 * @brief Encoder hardware definitions
 *
 * Parameters:
 * - id: Unique identifier (1-based for MIDI CC mapping)
 * - pinA/pinB: Quadrature signal pins
 * - ppr: Pulses per revolution (check encoder datasheet)
 * - rangeAngle: Rotation angle for full 0.0-1.0 range
 * - ticksPerEvent: Ticks per event emission (4 = one detent)
 * - invertDirection: Flip rotation direction if wired backwards
 */
constexpr std::array<oc::drivers::common::EncoderDef, 4> ENCODERS = {{
    // Encoder 1: MACRO_1 (pins from midi-studio/core)
    {
        .id = 1,
        .pinA = 22,
        .pinB = 23,
        .ppr = 24,
        .rangeAngle = 270,
        .ticksPerEvent = 4,
        .invertDirection = false
    },
    // Encoder 2: MACRO_2
    {
        .id = 2,
        .pinA = 18,
        .pinB = 19,
        .ppr = 24,
        .rangeAngle = 270,
        .ticksPerEvent = 4,
        .invertDirection = false
    },
    // Encoder 3: MACRO_3
    {
        .id = 3,
        .pinA = 40,
        .pinB = 41,
        .ppr = 24,
        .rangeAngle = 270,
        .ticksPerEvent = 4,
        .invertDirection = false
    },
    // Encoder 4: MACRO_4
    {
        .id = 4,
        .pinA = 36,
        .pinB = 37,
        .ppr = 24,
        .rangeAngle = 270,
        .ticksPerEvent = 4,
        .invertDirection = false
    }
}};

// ═══════════════════════════════════════════════════════════════════
// Button Configuration
// ═══════════════════════════════════════════════════════════════════

/**
 * @brief Button hardware definitions
 *
 * Parameters:
 * - id: Unique identifier
 * - pin: GPIO pin with source (MCU direct)
 * - activeLow: true = pressed when LOW (pull-up), false = pressed when HIGH
 */
constexpr std::array<oc::drivers::arduino::ButtonDef, 2> BUTTONS = {{
    // Button 1: NAV (pin from midi-studio/core)
    {
        .id = 1,
        .pin = {.pin = 32, .source = oc::hal::GpioPin::Source::MCU},
        .activeLow = true
    },
    // Button 2: AUX (available Teensy 4.1 pin)
    {
        .id = 2,
        .pin = {.pin = 35, .source = oc::hal::GpioPin::Source::MCU},
        .activeLow = true
    }
}};

// ═══════════════════════════════════════════════════════════════════
// MIDI Configuration
// ═══════════════════════════════════════════════════════════════════

/// MIDI channel (0-15, displayed as 1-16 in DAWs)
constexpr uint8_t MIDI_CHANNEL = 0;

/// Base CC number for encoders (encoder 1 = CC 16, encoder 2 = CC 17, etc.)
constexpr uint8_t ENCODER_CC_BASE = 16;

/// CC number for button 1
constexpr uint8_t BUTTON1_CC = 20;

/// CC number for button 2
constexpr uint8_t BUTTON2_CC = 21;

}  // namespace Config
