#pragma once

#include <array>
#include <memory>

#include <EncoderTool.h>

#include <drivers/common/EncoderDef.hpp>
#include <oc/core/input/EncoderLogic.hpp>
#include <oc/hal/IEncoderController.hpp>
#include <oc/hal/Types.hpp>

namespace oc::drivers::teensy {

/// Use common EncoderDef for GPIO-based encoders
using EncoderDef = common::EncoderDef;

/**
 * @brief Teensy encoder controller using EncoderTool library (ISR-based)
 *
 * High-performance driver optimized for Teensy 4.x using interrupt-driven
 * quadrature decoding via EncoderTool library.
 *
 * Processing model:
 * - ISR callback: Processes delta via EncoderLogic, sets pending flag
 * - update(): Flushes pending values via user callback (main loop safe)
 *
 * Modes:
 * - NORMALIZED: Position [0.0-1.0] mapped to bounds, ±1 per tick
 * - RAW: Raw tick position as float
 * - RELATIVE: Emits ±delta_per_detent when a full detent is reached
 *
 * @tparam N Number of encoders to manage
 *
 * @note Requires 'luni64/EncoderTool' in lib_deps
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
        // Create encoder logic instances (no hardware yet - lazy init)
        for (size_t i = 0; i < N; ++i) {
            core::input::EncoderConfig cfg{
                .id = defs[i].id,
                .ppr = defs[i].ppr,
                .rangeAngle = defs[i].rangeAngle,
                .ticksPerEvent = defs[i].ticksPerEvent,
                .invertDirection = defs[i].invertDirection
            };
            encoders_logic_[i] = std::make_unique<core::input::EncoderLogic>(cfg);
        }
    }

    ~EncoderController() override = default;

    EncoderController(const EncoderController&) = delete;
    EncoderController& operator=(const EncoderController&) = delete;
    EncoderController(EncoderController&&) = delete;
    EncoderController& operator=(EncoderController&&) = delete;

    bool init() override {
        if (initialized_) return true;

        for (size_t i = 0; i < N; ++i) {
            const auto& def = defs_[i];

            encoders_hw_[i] = std::make_unique<EncoderTool::Encoder>();
            encoders_hw_[i]->begin(def.pinA, def.pinB, EncoderTool::CountMode::full);

            // ISR callback - processes delta, sets pending flag
            // Safe: EncoderLogic uses std::atomic for pending flag
            encoders_hw_[i]->attachCallback([this, i](int, int delta) {
                encoders_logic_[i]->processDelta(delta);
            });
        }

        initialized_ = true;
        return true;
    }

    void update() override {
        if (!initialized_) return;

        // Flush pending values (main loop context - safe for callbacks)
        for (size_t i = 0; i < N; ++i) {
            auto pending = encoders_logic_[i]->flush();
            if (pending.has_value() && callback_) {
                callback_(defs_[i].id, pending.value());
            }
        }
    }

    float getPosition(hal::EncoderID id) const override {
        int idx = findIndex(id);
        if (idx < 0) return 0.0f;
        return encoders_logic_[idx]->getLastValue();
    }

    void setPosition(hal::EncoderID id, float value) override {
        int idx = findIndex(id);
        if (idx >= 0) encoders_logic_[idx]->setPosition(value);
    }

    void setMode(hal::EncoderID id, hal::EncoderMode mode) override {
        int idx = findIndex(id);
        if (idx >= 0) encoders_logic_[idx]->setMode(mode);
    }

    void setBounds(hal::EncoderID id, float min, float max) override {
        int idx = findIndex(id);
        if (idx >= 0) encoders_logic_[idx]->setBounds(min, max);
    }

    void setDiscreteSteps(hal::EncoderID id, uint8_t steps) override {
        int idx = findIndex(id);
        if (idx >= 0) encoders_logic_[idx]->setDiscreteSteps(steps);
    }

    void setContinuous(hal::EncoderID id) override {
        int idx = findIndex(id);
        if (idx >= 0) encoders_logic_[idx]->setContinuous();
    }

    void setDelta(hal::EncoderID id, float delta) override {
        int idx = findIndex(id);
        if (idx >= 0) encoders_logic_[idx]->setDelta(delta);
    }

    void setCallback(hal::EncoderCallback cb) override { callback_ = cb; }

private:
    int findIndex(hal::EncoderID id) const {
        for (size_t i = 0; i < N; ++i) {
            if (defs_[i].id == id) return static_cast<int>(i);
        }
        return -1;
    }

    std::array<EncoderDef, N> defs_;
    std::array<std::unique_ptr<EncoderTool::Encoder>, N> encoders_hw_;
    std::array<std::unique_ptr<core::input::EncoderLogic>, N> encoders_logic_;

    hal::EncoderCallback callback_;
    bool initialized_ = false;
};

}  // namespace oc::drivers::teensy
