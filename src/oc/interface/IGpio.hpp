#pragma once

#include <cstdint>

namespace oc::interface {

/**
 * @brief GPIO pin modes
 *
 * Prefixed with PIN_ to avoid conflict with Arduino macros (INPUT, OUTPUT, etc.)
 */
enum class PinMode : uint8_t {
    PIN_INPUT,
    PIN_INPUT_PULLUP,
    PIN_OUTPUT
};

/**
 * @brief Interface for GPIO hardware abstraction
 *
 * Platform-independent GPIO operations. Each HAL provides its implementation.
 */
class IGpio {
public:
    virtual ~IGpio() = default;

    virtual void pinMode(uint8_t pin, PinMode mode) = 0;
    virtual void digitalWrite(uint8_t pin, bool high) = 0;
    virtual bool digitalRead(uint8_t pin) = 0;
    virtual uint16_t analogRead(uint8_t pin) = 0;
};

}  // namespace oc::interface
