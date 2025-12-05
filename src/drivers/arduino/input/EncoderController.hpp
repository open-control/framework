#pragma once

// Dependency check (C++17 feature)
#if __has_include(<Encoder.h>)
#include <Encoder.h>
#else
#error "EncoderController requires Encoder library by PJRC. Add 'Encoder' to lib_deps in platformio.ini"
#endif

#include <array>
#include <memory>

#include <Arduino.h>

#include <drivers/common/EncoderLogic.hpp>
#include <oc/hal/IEncoderController.hpp>
#include <oc/hal/Types.hpp>

namespace oc::drivers::arduino {

/**
 * @brief Encoder definition for controller configuration
 *
 * @note See common::EncoderConfig for parameter documentation.
 *       invertDirection allows adapting to different hardware wiring.
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

/**
 * @brief Generic encoder controller using PJRC Encoder library
 *
 * Cross-platform driver working on AVR, ARM, ESP32, and other Arduino boards.
 * Uses polling-based quadrature decoding via PJRC Encoder library.
 *
 * Processing model (Core-compatible):
 * - update(): Reads position, processes ±1 per tick, flushes pending via callback
 *
 * Modes:
 * - NORMALIZED: Position [0.0-1.0] mapped to bounds, ±1 per tick
 * - RAW: Raw tick position as float
 * - RELATIVE: Emits ±delta_per_detent when a full detent is reached
 *
 * @tparam N Number of encoders to manage
 *
 * @note Requires 'Encoder' library by PJRC in lib_deps
 *
 * @code
 * constexpr std::array encoders = {
 *     EncoderDef{.id = 1, .pinA = 2, .pinB = 3},
 *     EncoderDef{.id = 2, .pinA = 4, .pinB = 5, .rangeAngle = 360},
 * };
 * EncoderController<2> ctrl(encoders);
 * ctrl.init();
 * ctrl.setCallback([](hal::EncoderID id, float value) { ... });
 * @endcode
 */
template <size_t N>
class EncoderController : public hal::IEncoderController {
public:
    explicit EncoderController(const std::array<EncoderDef, N>& defs) : defs_(defs) {
        // Create encoder logic instances
        for (size_t i = 0; i < N; ++i) {
            common::EncoderConfig cfg{
                .id = defs[i].id,
                .ppr = defs[i].ppr,
                .rangeAngle = defs[i].rangeAngle,
                .ticksPerEvent = defs[i].ticksPerEvent,
                .invertDirection = defs[i].invertDirection
            };
            encoders_logic_[i] = std::make_unique<common::EncoderLogic>(cfg);
        }
    }

    bool init() override {
        // Lazy init - create hardware Encoder objects here (not in constructor)
        // Reason: Arduino global objects are constructed before setup()
        for (size_t i = 0; i < N; ++i) {
            encoders_hw_[i] = std::make_unique<::Encoder>(defs_[i].pinA, defs_[i].pinB);
        }
        initialized_ = true;
        return true;
    }

    void update() override {
        if (!initialized_) return;

        for (size_t i = 0; i < N; ++i) {
            int32_t pos = encoders_hw_[i]->read();
            encoders_logic_[i]->processNewPosition(pos);

            // Flush pending value (safe: main loop context)
            auto pending = encoders_logic_[i]->flush();
            if (pending.has_value() && callback_) {
                callback_(defs_[i].id, pending.value());
            }
        }
    }

    float getPosition(hal::EncoderID id) const override {
        for (size_t i = 0; i < N; ++i) {
            if (defs_[i].id == id) {
                return encoders_logic_[i]->getLastValue();
            }
        }
        return 0.0f;
    }

    void setPosition(hal::EncoderID id, float value) override {
        for (size_t i = 0; i < N; ++i) {
            if (defs_[i].id == id) {
                int32_t ticks = encoders_logic_[i]->setPosition(value);
                if (encoders_hw_[i]) {
                    encoders_hw_[i]->write(ticks);
                }
                return;
            }
        }
    }

    void setMode(hal::EncoderID id, hal::EncoderMode mode) override {
        for (size_t i = 0; i < N; ++i) {
            if (defs_[i].id == id) {
                encoders_logic_[i]->setMode(mode);
                return;
            }
        }
    }

    void setBounds(hal::EncoderID id, float min, float max) override {
        for (size_t i = 0; i < N; ++i) {
            if (defs_[i].id == id) {
                encoders_logic_[i]->setBounds(min, max);
                return;
            }
        }
    }

    void setDiscreteSteps(hal::EncoderID id, uint8_t steps) override {
        for (size_t i = 0; i < N; ++i) {
            if (defs_[i].id == id) {
                encoders_logic_[i]->setDiscreteSteps(steps);
                return;
            }
        }
    }

    void setContinuous(hal::EncoderID id) override {
        for (size_t i = 0; i < N; ++i) {
            if (defs_[i].id == id) {
                encoders_logic_[i]->setContinuous();
                return;
            }
        }
    }

    void setDelta(hal::EncoderID id, float delta) override {
        for (size_t i = 0; i < N; ++i) {
            if (defs_[i].id == id) {
                encoders_logic_[i]->setDelta(delta);
                return;
            }
        }
    }

    void setCallback(hal::EncoderCallback cb) override { callback_ = cb; }

private:
    std::array<EncoderDef, N> defs_;
    std::array<std::unique_ptr<::Encoder>, N> encoders_hw_;               ///< Hardware (PJRC lib)
    std::array<std::unique_ptr<common::EncoderLogic>, N> encoders_logic_; ///< Logic (shared)

    hal::EncoderCallback callback_;
    bool initialized_ = false;
};

}  // namespace oc::drivers::arduino
