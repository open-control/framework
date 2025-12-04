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
    bool active_low = true;
};

/**
 * @brief Callback for button events
 */
using ButtonCallback = std::function<void(ButtonID, ButtonEvent)>;

/**
 * @brief Callback for encoder events
 *
 * @param id Encoder identifier
 * @param position Current absolute position
 * @param delta Change since last callback
 */
using EncoderCallback = std::function<void(EncoderID, int32_t position, int32_t delta)>;

}  // namespace oc::hal
