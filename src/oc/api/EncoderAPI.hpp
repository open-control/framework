#pragma once

#include <oc/core/input/EncoderBuilder.hpp>
#include <oc/core/struct/Binding.hpp>
#include <oc/hal/IEncoderController.hpp>
#include <oc/hal/Types.hpp>

namespace oc::core::input {
class InputBinding;
}

namespace oc::api {

/**
 * @brief API for encoder bindings and hardware state management
 *
 * Provides:
 * - Fluent binding API via encoder(id)
 * - Scope/binding cleanup
 * - Hardware state access (position, mode, bounds, etc.)
 *
 * @code
 * // Via IContext accessors:
 * onEncoder(ENC_1).turn().then([](float v){ updateValue(v); });
 * encoder(ENC_1).setPosition(0.5f);
 * encoder(ENC_1).setMode(EncoderMode::RELATIVE);
 * encoders().clearBindings();
 * @endcode
 */
class EncoderAPI {
public:
    EncoderAPI(core::input::InputBinding& binding, hal::IEncoderController& hw);

    // ═══════════════════════════════════════════════════
    // Binding fluent API
    // ═══════════════════════════════════════════════════

    /**
     * @brief Start building an encoder binding
     * @param id The encoder to bind
     * @return EncoderBuilder for chained configuration
     */
    [[nodiscard]] core::input::EncoderBuilder encoder(hal::EncoderID id);

    // ═══════════════════════════════════════════════════
    // Scope/cleanup
    // ═══════════════════════════════════════════════════

    /// Clear all encoder bindings
    void clearBindings();

    /// Clear encoder bindings in a specific scope
    void clearScope(core::ScopeID scope);

    // ═══════════════════════════════════════════════════
    // Hardware state
    // ═══════════════════════════════════════════════════

    /// Get current encoder position (value depends on mode)
    float getPosition(hal::EncoderID id) const;

    /// Set encoder position (value depends on mode)
    void setPosition(hal::EncoderID id, float value);

    /// Set encoder operating mode (NORMALIZED, RAW, RELATIVE)
    void setMode(hal::EncoderID id, hal::EncoderMode mode);

    /// Set encoder bounds for absolute mode
    void setBounds(hal::EncoderID id, float min, float max);

    /// Set delta per detent for relative mode
    void setDelta(hal::EncoderID id, float delta);

    /// Configure encoder for discrete steps
    void setDiscreteSteps(hal::EncoderID id, uint8_t steps);

    /// Configure encoder for continuous mode
    void setContinuous(hal::EncoderID id);

private:
    core::input::InputBinding& binding_;
    hal::IEncoderController& hw_;
};

}  // namespace oc::api
