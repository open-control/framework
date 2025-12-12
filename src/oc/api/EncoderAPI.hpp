#pragma once

#include <type_traits>

#include <oc/core/input/EncoderBuilder.hpp>
#include <oc/core/struct/Binding.hpp>
#include <oc/hal/IEncoderController.hpp>
#include <oc/hal/Types.hpp>

namespace oc::core::input {
class InputBinding;
}

namespace oc::api {

/// @brief Type trait helper to safely check underlying type (only for enums)
namespace detail {
template <typename T, bool IsEnum = std::is_enum_v<T>>
struct is_uint16_encoder_enum : std::false_type {};

template <typename T>
struct is_uint16_encoder_enum<T, true>
    : std::bool_constant<std::is_same_v<std::underlying_type_t<T>, uint16_t>> {};
}  // namespace detail

/// @brief Type trait to detect enum class with uint16_t underlying type
template <typename T>
inline constexpr bool is_encoder_id_v = detail::is_uint16_encoder_enum<T>::value;

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
     * @param id The encoder to bind (uint16_t or enum class : uint16_t)
     * @return EncoderBuilder for chained configuration
     */
    [[nodiscard]] core::input::EncoderBuilder encoder(hal::EncoderID id);

    /// @brief Template overload for enum class encoder IDs
    template <typename EnumT, typename = std::enable_if_t<is_encoder_id_v<EnumT>>>
    [[nodiscard]] core::input::EncoderBuilder encoder(EnumT id) {
        return encoder(static_cast<hal::EncoderID>(id));
    }

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
    template <typename EnumT, typename = std::enable_if_t<is_encoder_id_v<EnumT>>>
    float getPosition(EnumT id) const { return getPosition(static_cast<hal::EncoderID>(id)); }

    /// Set encoder position (value depends on mode)
    void setPosition(hal::EncoderID id, float value);
    template <typename EnumT, typename = std::enable_if_t<is_encoder_id_v<EnumT>>>
    void setPosition(EnumT id, float value) { setPosition(static_cast<hal::EncoderID>(id), value); }

    /// Set encoder operating mode (NORMALIZED, RAW, RELATIVE)
    void setMode(hal::EncoderID id, hal::EncoderMode mode);
    template <typename EnumT, typename = std::enable_if_t<is_encoder_id_v<EnumT>>>
    void setMode(EnumT id, hal::EncoderMode mode) { setMode(static_cast<hal::EncoderID>(id), mode); }

    /// Set encoder bounds for absolute mode
    void setBounds(hal::EncoderID id, float min, float max);
    template <typename EnumT, typename = std::enable_if_t<is_encoder_id_v<EnumT>>>
    void setBounds(EnumT id, float min, float max) { setBounds(static_cast<hal::EncoderID>(id), min, max); }

    /// Set delta per detent for relative mode
    void setDelta(hal::EncoderID id, float delta);
    template <typename EnumT, typename = std::enable_if_t<is_encoder_id_v<EnumT>>>
    void setDelta(EnumT id, float delta) { setDelta(static_cast<hal::EncoderID>(id), delta); }

    /// Configure encoder for discrete steps
    void setDiscreteSteps(hal::EncoderID id, uint8_t steps);
    template <typename EnumT, typename = std::enable_if_t<is_encoder_id_v<EnumT>>>
    void setDiscreteSteps(EnumT id, uint8_t steps) { setDiscreteSteps(static_cast<hal::EncoderID>(id), steps); }

    /// Configure encoder for continuous mode
    void setContinuous(hal::EncoderID id);
    template <typename EnumT, typename = std::enable_if_t<is_encoder_id_v<EnumT>>>
    void setContinuous(EnumT id) { setContinuous(static_cast<hal::EncoderID>(id)); }

private:
    core::input::InputBinding& binding_;
    hal::IEncoderController& hw_;
};

}  // namespace oc::api
