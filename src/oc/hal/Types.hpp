#pragma once

#include <cstdint>
#include <functional>

namespace oc::hal {

/**
 * @brief Generic button identifier
 *
 * Consumer projects define their own enum class inheriting from this type.
 */
using ButtonID = uint16_t;

/**
 * @brief Generic encoder identifier
 *
 * Consumer projects define their own enum class inheriting from this type.
 */
using EncoderID = uint16_t;

/**
 * @brief Button event types
 */
enum class ButtonEvent : uint8_t {
    PRESSED,
    RELEASED
};

/**
 * @brief Rectangle for display regions
 */
struct Rect {
    int16_t x1;
    int16_t y1;
    int16_t x2;
    int16_t y2;
};

/**
 * @brief GPIO pin configuration
 */
struct GpioPin {
    enum class Source : uint8_t {
        MCU,  ///< Direct MCU GPIO
        MUX   ///< Via multiplexer
    };

    uint8_t pin;
    Source source = Source::MCU;
};

/**
 * @brief Callback for button events
 */
using ButtonCallback = std::function<void(ButtonID, ButtonEvent)>;

/**
 * @brief Callback for encoder events
 *
 * @param id Encoder identifier
 * @param value Value depends on EncoderMode:
 *              - NORMALIZED: position [0.0-1.0] based on bounds
 *              - RAW: hardware ticks (as float)
 *              - RELATIVE: delta per detent (e.g., +1.0 or -1.0)
 */
using EncoderCallback = std::function<void(EncoderID id, float value)>;

/**
 * @brief Function pointer for time source (milliseconds)
 *
 * Allows framework to be platform-agnostic. Default implementations:
 * - Arduino: millis()
 * - FreeRTOS: xTaskGetTickCount() * portTICK_PERIOD_MS
 * - Bare metal: custom SysTick handler
 */
using TimeProvider = uint32_t (*)();

}  // namespace oc::hal
