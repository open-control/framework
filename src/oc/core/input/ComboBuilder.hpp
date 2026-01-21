#pragma once

#include <oc/core/input/BindingHandle.hpp>
#include <oc/core/input/Traits.hpp>
#include <oc/core/input/Binding.hpp>
#include <oc/type/Ids.hpp>
#include <oc/type/Callbacks.hpp>

namespace oc::core::input {

// Forward declaration
class InputBinding;

/**
 * @brief Fluent builder for combo button bindings
 *
 * Created via ButtonBuilder::combo(). Triggers when both buttons are pressed.
 *
 * Usage:
 * @code
 * api.button(BTN_1).combo(BTN_2).then([]{ resetAll(); });
 * api.button(BTN_1).combo(BTN_2).scope(s).then([]{ doAction(); });
 * @endcode
 */
class [[nodiscard]] ComboBuilder {
public:
    /**
     * @brief Construct a combo builder
     * @param registry The InputBinding that will own the binding
     * @param btn1 First button in the combo
     * @param btn2 Second button in the combo
     */
    ComboBuilder(InputBinding* registry, oc::type::ButtonID btn1, oc::type::ButtonID btn2);

    /**
     * @brief Destructor warns if then() was never called
     */
    ~ComboBuilder();

    // Move-only
    ComboBuilder(ComboBuilder&& other) noexcept;
    ComboBuilder& operator=(ComboBuilder&& other) noexcept;
    ComboBuilder(const ComboBuilder&) = delete;
    ComboBuilder& operator=(const ComboBuilder&) = delete;

    // ═══════════════════════════════════════════════════
    // Modifiers (optional, chainable)
    // ═══════════════════════════════════════════════════

    /**
     * @brief Set the scope ID for this binding
     * @param s Scope identifier (non-zero for scoped bindings)
     */
    ComboBuilder& scope(oc::type::ScopeID s);

    /**
     * @brief Set scope from a duck-typed provider
     *
     * @tparam T Provider type (e.g., LVGLScope)
     * @param provider The scope provider
     */
    template <typename T>
    ComboBuilder& scope(const T& provider) {
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
    ComboBuilder& when(oc::type::IsActiveFn fn);

    // ═══════════════════════════════════════════════════
    // Terminal (MUST be called)
    // ═══════════════════════════════════════════════════

    /**
     * @brief Register the binding with the specified callback
     * @param cb Action to execute when both buttons are pressed
     * @return Handle for optional unbinding
     */
    BindingHandle then(oc::type::ActionCallback cb);

private:
    InputBinding* registry_ = nullptr;
    oc::type::ButtonID btn1_{};
    oc::type::ButtonID btn2_{};
    oc::type::ScopeID scope_ = 0;
    oc::type::IsActiveFn isActive_ = nullptr;
    bool finalized_ = false;
};

}  // namespace oc::core::input
