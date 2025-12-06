#pragma once

#include <cstdint>

namespace oc::hal {

/**
 * @brief GPIO pin modes
 *
 * Minimal contract supported by all target platforms:
 * - Teensy, ESP32, STM32, RP2040, AVR
 */
enum class PinMode : uint8_t {
    INPUT,
    INPUT_PULLUP,
    OUTPUT
};

/**
 * @brief Interface for GPIO hardware abstraction
 *
 * Platform-independent GPIO operations. Each HAL provides its implementation.
 *
 * @note This interface does NOT include timing functions (delay).
 *       Each HAL uses its platform's native timing for internal operations.
 */
class IGpio {
public:
    virtual ~IGpio() = default;

    /**
     * @brief Configure pin mode
     * @param pin GPIO pin number
     * @param mode Pin mode (INPUT, INPUT_PULLUP, OUTPUT)
     */
    virtual void pinMode(uint8_t pin, PinMode mode) = 0;

    /**
     * @brief Write digital value to pin
     * @param pin GPIO pin number
     * @param high true for HIGH, false for LOW
     */
    virtual void digitalWrite(uint8_t pin, bool high) = 0;

    /**
     * @brief Read digital value from pin
     * @param pin GPIO pin number
     * @return true if HIGH, false if LOW
     */
    virtual bool digitalRead(uint8_t pin) = 0;

    /**
     * @brief Read analog value from pin
     * @param pin GPIO/ADC pin number
     * @return ADC value (resolution depends on platform, typically 10-12 bits)
     */
    virtual uint16_t analogRead(uint8_t pin) = 0;
};

}  // namespace oc::hal
