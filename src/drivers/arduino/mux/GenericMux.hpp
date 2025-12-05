#pragma once

#include <array>

#include <Arduino.h>

#include <oc/hal/IMultiplexer.hpp>

namespace oc::drivers::arduino {

/**
 * @brief Generic multiplexer driver for CD74HC40xx series
 *
 * Supports any multiplexer with 1-4 select pins (2-16 channels).
 * Works on all Arduino-compatible boards.
 *
 * @tparam NumPins Number of select pins (1-4)
 *
 * @code
 * // 16-channel mux (CD74HC4067)
 * CD74HC4067::Config cfg{
 *     .selectPins = {2, 3, 4, 5},
 *     .signalPin = A0,
 *     .settleTimeUs = 20
 * };
 * CD74HC4067 mux(cfg);
 * mux.init();
 * bool pressed = mux.readDigital(0);
 * @endcode
 */
template <uint8_t NumPins>
class GenericMux : public hal::IMultiplexer {
    static_assert(NumPins >= 1 && NumPins <= 4, "Mux supports 1-4 select pins");

public:
    /**
     * @brief Multiplexer configuration
     */
    struct Config {
        std::array<uint8_t, NumPins> selectPins;  ///< GPIO pins for channel selection
        uint8_t signalPin;                        ///< Signal/common pin
        uint16_t settleTimeUs = 20;               ///< Settling time after channel switch
        bool signalPullup = true;                 ///< Enable internal pullup on signal pin
    };

    explicit GenericMux(const Config& cfg) : config_(cfg) {}

    bool init() override {
        for (uint8_t pin : config_.selectPins) {
            pinMode(pin, OUTPUT);
            digitalWrite(pin, LOW);
        }
        pinMode(config_.signalPin, config_.signalPullup ? INPUT_PULLUP : INPUT);
        current_channel_ = 0;
        initialized_ = true;
        return true;
    }

    uint8_t channelCount() const override { return 1 << NumPins; }

    void select(uint8_t channel) override {
        if (!initialized_ || channel >= channelCount()) return;
        if (channel == current_channel_) return;

        for (uint8_t i = 0; i < NumPins; ++i) {
            digitalWrite(config_.selectPins[i], (channel >> i) & 0x01);
        }
        current_channel_ = channel;
        delayMicroseconds(config_.settleTimeUs);
    }

    bool readDigital(uint8_t channel) override {
        select(channel);
        return digitalRead(config_.signalPin);
    }

    uint16_t readAnalog(uint8_t channel) override {
        select(channel);
        return analogRead(config_.signalPin);
    }

    bool supportsAnalog() const override { return true; }

private:
    Config config_;
    uint8_t current_channel_ = 0;
    bool initialized_ = false;
};

// =============================================================================
// Pre-configured aliases for common multiplexer ICs
// =============================================================================

using CD74HC4067 = GenericMux<4>;  ///< 16 channels (4 select pins)
using CD74HC4051 = GenericMux<3>;  ///< 8 channels (3 select pins)
using CD74HC4052 = GenericMux<2>;  ///< 4 channels (2 select pins)

}  // namespace oc::drivers::arduino
