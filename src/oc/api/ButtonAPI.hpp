#pragma once

#include <oc/core/input/ButtonBuilder.hpp>
#include <oc/core/struct/Binding.hpp>
#include <oc/hal/Types.hpp>

namespace oc::core::input {
class InputBinding;
}

namespace oc::api {

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
 * button(BTN_1).setLatch(true);
 * buttons().clearBindings();
 * @endcode
 */
class ButtonAPI {
public:
    explicit ButtonAPI(core::input::InputBinding& binding);

    // ═══════════════════════════════════════════════════
    // Binding fluent API
    // ═══════════════════════════════════════════════════

    /**
     * @brief Start building a button binding
     * @param id The button to bind
     * @return ButtonBuilder for chained configuration
     */
    [[nodiscard]] core::input::ButtonBuilder button(hal::ButtonID id);

    // ═══════════════════════════════════════════════════
    // Scope/cleanup
    // ═══════════════════════════════════════════════════

    /// Clear all button bindings
    void clearBindings();

    /// Clear button bindings in a specific scope
    void clearScope(core::ScopeID scope);

    // ═══════════════════════════════════════════════════
    // Button state
    // ═══════════════════════════════════════════════════

    /// Check if button is currently pressed (instantaneous state)
    bool isPressed(hal::ButtonID id) const;

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
    core::IsActiveFn pressed(hal::ButtonID id) const;

    // ═══════════════════════════════════════════════════
    // Latch state
    // ═══════════════════════════════════════════════════

    /// Check if button is in latched state
    bool isLatched(hal::ButtonID id) const;

    /// Set button latch state
    void setLatch(hal::ButtonID id, bool latched);

private:
    core::input::InputBinding& binding_;
};

}  // namespace oc::api
