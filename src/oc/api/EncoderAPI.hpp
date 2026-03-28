#pragma once

#include <oc/core/input/EncoderBuilder.hpp>
#include <oc/core/input/Binding.hpp>
#include <oc/interface/IEncoder.hpp>
#include <oc/type/Ids.hpp>
#include <oc/type/Callbacks.hpp>

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
 * encoder(ENC_1).setMode(interface::EncoderMode::RELATIVE);
 * encoders().clearBindings();
 * @endcode
 */
class EncoderAPI {
public:
    EncoderAPI(core::input::InputBinding& binding, interface::IEncoder& hw);

    // ═══════════════════════════════════════════════════
    // Binding fluent API
    // ═══════════════════════════════════════════════════

    /**
     * @brief Start building an encoder binding
     * @param id The encoder to bind (uint16_t or enum class : uint16_t)
     * @return EncoderBuilder for chained configuration
     */
    [[nodiscard]] core::input::EncoderBuilder encoder(oc::type::EncoderID id);

    /// @brief Template overload for enum class encoder IDs
    template <typename EnumT, typename = std::enable_if_t<oc::type::is_id_v<EnumT>>>
    [[nodiscard]] core::input::EncoderBuilder encoder(EnumT id) {
        return encoder(static_cast<oc::type::EncoderID>(id));
    }

    // ═══════════════════════════════════════════════════
    // Scope/cleanup
    // ═══════════════════════════════════════════════════

    /// Clear all encoder bindings
    void clearBindings();

    /// Clear encoder bindings in a specific scope
    void clearScope(oc::type::ScopeID scope);

    // ═══════════════════════════════════════════════════
    // Hardware state
    // ═══════════════════════════════════════════════════

    /// Get current encoder position (value depends on mode)
    float getPosition(oc::type::EncoderID id) const;
    template <typename EnumT, typename = std::enable_if_t<oc::type::is_id_v<EnumT>>>
    float getPosition(EnumT id) const { return getPosition(static_cast<oc::type::EncoderID>(id)); }

    /// Set encoder position (value depends on mode)
    void setPosition(oc::type::EncoderID id, float value);
    template <typename EnumT, typename = std::enable_if_t<oc::type::is_id_v<EnumT>>>
    void setPosition(EnumT id, float value) { setPosition(static_cast<oc::type::EncoderID>(id), value); }

    /// Set encoder operating mode (NORMALIZED, RAW, RELATIVE)
    void setMode(oc::type::EncoderID id, interface::EncoderMode mode);
    template <typename EnumT, typename = std::enable_if_t<oc::type::is_id_v<EnumT>>>
    void setMode(EnumT id, interface::EncoderMode mode) { setMode(static_cast<oc::type::EncoderID>(id), mode); }

    /// Set encoder bounds for absolute mode
    void setBounds(oc::type::EncoderID id, float min, float max);
    template <typename EnumT, typename = std::enable_if_t<oc::type::is_id_v<EnumT>>>
    void setBounds(EnumT id, float min, float max) { setBounds(static_cast<oc::type::EncoderID>(id), min, max); }

    /// Set delta per detent for relative mode
    void setDelta(oc::type::EncoderID id, float delta);
    template <typename EnumT, typename = std::enable_if_t<oc::type::is_id_v<EnumT>>>
    void setDelta(EnumT id, float delta) { setDelta(static_cast<oc::type::EncoderID>(id), delta); }

    /// Configure encoder for discrete steps
    void setDiscreteSteps(oc::type::EncoderID id, uint8_t steps);
    template <typename EnumT, typename = std::enable_if_t<oc::type::is_id_v<EnumT>>>
    void setDiscreteSteps(EnumT id, uint8_t steps) { setDiscreteSteps(static_cast<oc::type::EncoderID>(id), steps); }

    /// Configure the number of virtual ticks required per discrete step
    void setDiscreteTicksPerStep(oc::type::EncoderID id, uint16_t ticksPerStep);
    template <typename EnumT, typename = std::enable_if_t<oc::type::is_id_v<EnumT>>>
    void setDiscreteTicksPerStep(EnumT id, uint16_t ticksPerStep) {
        setDiscreteTicksPerStep(static_cast<oc::type::EncoderID>(id), ticksPerStep);
    }

    /// Override normalized full-scale travel in turns (0 = hardware default)
    void setNormalizedTurns(oc::type::EncoderID id, float turns);
    template <typename EnumT, typename = std::enable_if_t<oc::type::is_id_v<EnumT>>>
    void setNormalizedTurns(EnumT id, float turns) {
        setNormalizedTurns(static_cast<oc::type::EncoderID>(id), turns);
    }

    /// Configure encoder for continuous mode
    void setContinuous(oc::type::EncoderID id);
    template <typename EnumT, typename = std::enable_if_t<oc::type::is_id_v<EnumT>>>
    void setContinuous(EnumT id) { setContinuous(static_cast<oc::type::EncoderID>(id)); }

private:
    core::input::InputBinding& binding_;
    interface::IEncoder& hw_;
};

}  // namespace oc::api
