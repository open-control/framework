#pragma once

#include <oc/core/input/BindingHandle.hpp>
#include <oc/core/input/Traits.hpp>
#include <oc/core/input/Binding.hpp>
#include <oc/type/Ids.hpp>
#include <oc/type/Callbacks.hpp>

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
    ButtonBuilder(InputBinding* registry, oc::type::ButtonID buttonId);

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
    ComboBuilder combo(oc::type::ButtonID other);

    // ═══════════════════════════════════════════════════
    // Modifiers (optional, chainable)
    // ═══════════════════════════════════════════════════

    /**
     * @brief Set the scope ID for this binding
     * @param s Scope identifier (non-zero for scoped bindings)
     */
    ButtonBuilder& scope(oc::type::ScopeID s);

    /**
     * @brief Set scope from a duck-typed provider
     *
     * The provider must have:
     * - getScopeID() const -> oc::type::ScopeID (required)
     * - getIsActive() const -> oc::type::IsActiveFn (optional)
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
    ButtonBuilder& when(oc::type::IsActiveFn fn);

    /**
     * @brief Enable latch (toggle) behavior
     *
     * When latched, first press triggers action and latches,
     * second press releases latch.
     */
    ButtonBuilder& latch();

    /**
     * @brief Reserve this global binding across every active scope
     *
     * This is intended for invariant hardware controls such as transport.
     * It is invalid on a scoped binding.
     */
    ButtonBuilder& globalPassThrough();

    /**
     * @brief Set deterministic precedence within a scope
     */
    ButtonBuilder& priority(int8_t value);

    // ═══════════════════════════════════════════════════
    // Terminal (MUST be called)
    // ═══════════════════════════════════════════════════

    /**
     * @brief Register the binding with the specified callback
     * @param cb Action to execute when binding triggers
     * @return Handle for optional unbinding
     */
    BindingHandle then(oc::type::ActionCallback cb);

private:
    InputBinding* registry_ = nullptr;
    oc::type::ButtonID buttonId_{};
    ButtonBindingType type_ = ButtonBindingType::PRESS;
    uint32_t timingMs_ = 0;
    oc::type::ScopeID scope_ = 0;
    oc::type::IsActiveFn isActive_ = nullptr;
    bool latch_ = false;
    bool globalPassThrough_ = false;
    int8_t priority_ = 0;
    bool gestureSet_ = false;
    bool finalized_ = false;
};

}  // namespace oc::core::input
