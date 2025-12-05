#pragma once

#include <cstdint>

#include <oc/hal/Types.hpp>

namespace oc::drivers::common {

/**
 * @brief Common encoder definition for GPIO-based encoders
 *
 * Shared by drivers that use simple GPIO pin pairs (Arduino, Teensy, etc.).
 * Platforms with different GPIO models (e.g., port-based) may define their own.
 *
 * @note Logic parameters (ppr, rangeAngle, etc.) are passed to EncoderLogic.
 *       Pin parameters (pinA, pinB) are used by the hardware driver.
 */
struct EncoderDef {
    hal::EncoderID id;              ///< Unique encoder identifier
    uint8_t pinA;                   ///< Quadrature signal A pin
    uint8_t pinB;                   ///< Quadrature signal B pin
    uint16_t ppr = 24;              ///< Pulses per revolution
    uint16_t rangeAngle = 270;      ///< Degrees for full [0..1] range
    uint8_t ticksPerEvent = 4;      ///< Ticks before event emission (4 = one detent)
    bool invertDirection = false;   ///< Invert rotation direction
};

}  // namespace oc::drivers::common
