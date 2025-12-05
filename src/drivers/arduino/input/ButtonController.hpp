#pragma once

#include <array>

#include <Arduino.h>

#include <oc/hal/IButtonController.hpp>
#include <oc/hal/IMultiplexer.hpp>
#include <oc/hal/Types.hpp>

namespace oc::drivers::arduino {

/**
 * @brief Button definition for controller configuration
 */
struct ButtonDef {
    hal::ButtonID id;    ///< Unique button identifier
    hal::GpioPin pin;    ///< GPIO pin configuration (MCU or MUX)
    bool activeLow = true;  ///< True if button pulls LOW when pressed (default)
};

/**
 * @brief Generic button controller with debouncing
 *
 * Supports both direct MCU GPIO and multiplexed buttons.
 * Works on all Arduino-compatible boards.
 *
 * @tparam N Number of buttons to manage
 *
 * @code
 * constexpr std::array buttons = {
 *     ButtonDef{1, {6, hal::GpioPin::Source::MCU}},
 *     ButtonDef{2, {0, hal::GpioPin::Source::MUX}},  // MUX channel 0
 * };
 * ButtonController<2> ctrl(buttons, &mux);
 * ctrl.init();
 * ctrl.setCallback([](hal::ButtonID id, hal::ButtonEvent evt) { ... });
 * @endcode
 */
template <size_t N>
class ButtonController : public hal::IButtonController {
public:
    /**
     * @brief Construct a button controller
     *
     * @param buttons Array of button definitions
     * @param mux Optional multiplexer for MUX-sourced buttons (nullptr if not used)
     * @param debounceMs Debounce time in milliseconds
     */
    ButtonController(
        const std::array<ButtonDef, N>& buttons,
        hal::IMultiplexer* mux = nullptr,
        uint8_t debounceMs = 5)
        : buttons_(buttons), mux_(mux), debounce_ms_(debounceMs) {
        states_.fill(false);
        last_change_.fill(0);
    }

    bool init() override {
        for (const auto& btn : buttons_) {
            if (btn.pin.source == hal::GpioPin::Source::MCU) {
                pinMode(btn.pin.pin, INPUT_PULLUP);
            }
        }
        initialized_ = true;
        return true;
    }

    void update() override {
        if (!initialized_) return;

        uint32_t now = millis();

        for (size_t i = 0; i < N; ++i) {
            bool raw = readPin(buttons_[i]);
            bool pressed = buttons_[i].activeLow ? !raw : raw;

            if (pressed != states_[i]) {
                if (now - last_change_[i] >= debounce_ms_) {
                    states_[i] = pressed;
                    last_change_[i] = now;

                    if (callback_) {
                        callback_(
                            buttons_[i].id,
                            pressed ? hal::ButtonEvent::PRESSED
                                    : hal::ButtonEvent::RELEASED);
                    }
                }
            }
        }
    }

    bool isPressed(hal::ButtonID id) const override {
        for (size_t i = 0; i < N; ++i) {
            if (buttons_[i].id == id) return states_[i];
        }
        return false;
    }

    void setCallback(hal::ButtonCallback cb) override { callback_ = cb; }

private:
    bool readPin(const ButtonDef& btn) {
        if (btn.pin.source == hal::GpioPin::Source::MCU) {
            return digitalRead(btn.pin.pin);
        } else {
            // MUX source - pin.pin is the channel number
            if (mux_) {
                return mux_->readDigital(btn.pin.pin);
            }
            return false;  // No mux configured
        }
    }

    std::array<ButtonDef, N> buttons_;
    hal::IMultiplexer* mux_;
    uint8_t debounce_ms_;

    std::array<bool, N> states_;
    std::array<uint32_t, N> last_change_;
    hal::ButtonCallback callback_;
    bool initialized_ = false;
};

}  // namespace oc::drivers::arduino
