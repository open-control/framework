#pragma once

#include <type_traits>

#include <oc/core/input/AuthorityResolver.hpp>
#include <oc/core/input/ButtonBuilder.hpp>
#include <oc/core/struct/Binding.hpp>
#include <oc/interface/IButton.hpp>
#include <oc/types/Ids.hpp>
#include <oc/types/Callbacks.hpp>

namespace oc::core::input {
class InputBinding;
}

namespace oc::api {

/// @brief Type trait helper to safely check underlying type (only for enums)
namespace detail {
template <typename T, bool IsEnum = std::is_enum_v<T>>
struct is_uint16_enum : std::false_type {};

template <typename T>
struct is_uint16_enum<T, true>
    : std::bool_constant<std::is_same_v<std::underlying_type_t<T>, uint16_t>> {};
}  // namespace detail

/// @brief Type trait to detect enum class with uint16_t underlying type
template <typename T>
inline constexpr bool is_button_id_v = detail::is_uint16_enum<T>::value;

/**
 * @brief API for button bindings and state management
 *
 * Provides:
 * - Fluent binding API via button(id)
 * - Scope/binding cleanup
 * - Button state queries (isPressed, isLatched)
 * - Latch state management
 *
 * @code
 * // Via IContext accessors:
 * onButton(BTN_1).press().then([this]{ doAction(); });
 * button(BTN_1).clearLatch();  // Clear latch for a button
 * buttons().clearBindings();
 * @endcode
 */
class ButtonAPI {
public:
    ButtonAPI(core::input::InputBinding& binding, interface::IButton& hw);

    // ═══════════════════════════════════════════════════
    // Binding fluent API
    // ═══════════════════════════════════════════════════

    /**
     * @brief Start building a button binding
     * @param id The button to bind (uint16_t or enum class : uint16_t)
     * @return ButtonBuilder for chained configuration
     */
    [[nodiscard]] core::input::ButtonBuilder button(ButtonID id);

    /// @brief Template overload for enum class button IDs
    template <typename EnumT, typename = std::enable_if_t<is_button_id_v<EnumT>>>
    [[nodiscard]] core::input::ButtonBuilder button(EnumT id) {
        return button(static_cast<ButtonID>(id));
    }

    // ═══════════════════════════════════════════════════
    // Scope/cleanup
    // ═══════════════════════════════════════════════════

    /// Clear all button bindings
    void clearBindings();

    /// Clear button bindings in a specific scope
    void clearScope(oc::ScopeID scope);

    /**
     * @brief Set the authority resolver for scope-based input filtering
     *
     * When set, scoped bindings will only trigger if their scope has authority.
     * This prevents multiple overlays from receiving the same input events.
     *
     * @param resolver Pointer to the authority resolver (nullptr to disable)
     */
    void setAuthorityResolver(const core::input::AuthorityResolver* resolver);

    // ═══════════════════════════════════════════════════
    // Button state
    // ═══════════════════════════════════════════════════

    /// Check if button is currently pressed (instantaneous state)
    bool isPressed(ButtonID id) const;
    template <typename EnumT, typename = std::enable_if_t<is_button_id_v<EnumT>>>
    bool isPressed(EnumT id) const { return isPressed(static_cast<ButtonID>(id)); }

    /**
     * @brief Get a predicate for use with when()
     * @param id The button to check
     * @return IsActiveFn that returns true when button is pressed
     *
     * @code
     * onEncoder(ENC_1).turn()
     *     .when(button(BTN_SHIFT).pressed())
     *     .then([](float v){ fineAdjust(v); });
     * @endcode
     */
    core::IsActiveFn pressed(ButtonID id) const;
    template <typename EnumT, typename = std::enable_if_t<is_button_id_v<EnumT>>>
    core::IsActiveFn pressed(EnumT id) const { return pressed(static_cast<ButtonID>(id)); }

    // ═══════════════════════════════════════════════════
    // Latch state
    // ═══════════════════════════════════════════════════

    /// Check if button is in latched state
    bool isLatched(ButtonID id) const;
    template <typename EnumT, typename = std::enable_if_t<is_button_id_v<EnumT>>>
    bool isLatched(EnumT id) const { return isLatched(static_cast<ButtonID>(id)); }

    /// Clear button latch state
    void clearLatch(ButtonID id);
    template <typename EnumT, typename = std::enable_if_t<is_button_id_v<EnumT>>>
    void clearLatch(EnumT id) { clearLatch(static_cast<ButtonID>(id)); }

private:
    core::input::InputBinding& binding_;
    interface::IButton& hw_;
};

}  // namespace oc::api
