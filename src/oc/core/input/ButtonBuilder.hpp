#pragma once

#include <oc/core/input/BindingHandle.hpp>
#include <oc/core/input/Traits.hpp>
#include <oc/core/input/Binding.hpp>
#include <oc/types/Ids.hpp>
#include <oc/types/Callbacks.hpp>

namespace oc::core::input {

// Forward declarations
class InputBinding;
class ComboBuilder;

/**
 * @brief Fluent builder for button bindings
 *
 * Provides a type-safe, chainable API for creating button bindings.
 * Must call a gesture method (press, release, etc.) and terminate with then().
 *
 * Usage:
 * @code
 * onButton(BTN_1).press().then([]{ doAction(); });
 * onButton(BTN_1).longPress(800).scope(s).then([]{ showMenu(); });
 * onButton(BTN_1).combo(BTN_2).then([]{ reset(); });
 * @endcode
 *
 * @note [[nodiscard]] ensures the builder is not discarded without calling then()
 */
class [[nodiscard]] ButtonBuilder {
public:
    /**
     * @brief Construct a button builder
     * @param registry The InputBinding that will own the binding
     * @param buttonId The button this binding applies to
     */
    ButtonBuilder(InputBinding* registry, ButtonID buttonId);

    /**
     * @brief Destructor warns if then() was never called
     */
    ~ButtonBuilder();

    // Move-only (for return from factory)
    ButtonBuilder(ButtonBuilder&& other) noexcept;
    ButtonBuilder& operator=(ButtonBuilder&& other) noexcept;
    ButtonBuilder(const ButtonBuilder&) = delete;
    ButtonBuilder& operator=(const ButtonBuilder&) = delete;

    // ═══════════════════════════════════════════════════
    // Gesture selection (exactly ONE required)
    // ═══════════════════════════════════════════════════

    /**
     * @brief Trigger on button press
     */
    ButtonBuilder& press();

    /**
     * @brief Trigger on button release
     */
    ButtonBuilder& release();

    /**
     * @brief Trigger after holding button for specified duration
     * @param ms Duration in milliseconds (0 = use config default)
     */
    ButtonBuilder& longPress(uint32_t ms = 0);

    /**
     * @brief Trigger on rapid double press
     * @param ms Window in milliseconds (0 = use config default)
     */
    ButtonBuilder& doubleTap(uint32_t ms = 0);

    /**
     * @brief Create a combo binding with another button
     * @param other The second button in the combo
     * @return A ComboBuilder for the combo binding
     */
    ComboBuilder combo(ButtonID other);

    // ═══════════════════════════════════════════════════
    // Modifiers (optional, chainable)
    // ═══════════════════════════════════════════════════

    /**
     * @brief Set the scope ID for this binding
     * @param s Scope identifier (non-zero for scoped bindings)
     */
    ButtonBuilder& scope(ScopeID s);

    /**
     * @brief Set scope from a duck-typed provider
     *
     * The provider must have:
     * - getScopeID() const -> ScopeID (required)
     * - getIsActive() const -> IsActiveFn (optional)
     *
     * @tparam T Provider type (e.g., LVGLScope)
     * @param provider The scope provider
     */
    template <typename T>
    ButtonBuilder& scope(const T& provider) {
        scope_ = provider.getScopeID();
        if constexpr (has_getIsActive<T>::value) {
            isActive_ = provider.getIsActive();
        }
        return *this;
    }

    /**
     * @brief Set custom activation condition
     * @param fn Function returning true when binding should be active
     */
    ButtonBuilder& when(IsActiveFn fn);

    /**
     * @brief Enable latch (toggle) behavior
     *
     * When latched, first press triggers action and latches,
     * second press releases latch.
     */
    ButtonBuilder& latch();

    // ═══════════════════════════════════════════════════
    // Terminal (MUST be called)
    // ═══════════════════════════════════════════════════

    /**
     * @brief Register the binding with the specified callback
     * @param cb Action to execute when binding triggers
     * @return Handle for optional unbinding
     */
    BindingHandle then(ActionCallback cb);

private:
    InputBinding* registry_ = nullptr;
    ButtonID buttonId_{};
    ButtonBindingType type_ = ButtonBindingType::PRESS;
    uint32_t timingMs_ = 0;
    ScopeID scope_ = 0;
    IsActiveFn isActive_ = nullptr;
    bool latch_ = false;
    bool gestureSet_ = false;
    bool finalized_ = false;
};

}  // namespace oc::core::input
