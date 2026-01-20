#pragma once

#include <type_traits>
#include <utility>

#include <oc/core/Warning.hpp>
#include <oc/core/input/BindingHandle.hpp>
#include <oc/core/struct/Binding.hpp>
#include <oc/interface/Types.hpp>

namespace oc::core::input {

// Forward declaration
class InputBinding;

// SFINAE helper
template <typename T, typename = void>
struct has_getIsActive_combo : std::false_type {};

template <typename T>
struct has_getIsActive_combo<T, std::void_t<decltype(std::declval<const T&>().getIsActive())>>
    : std::true_type {};

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
    ComboBuilder(InputBinding* registry, ButtonID btn1, ButtonID btn2);

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
    ComboBuilder& scope(ScopeID s);

    /**
     * @brief Set scope from a duck-typed provider
     *
     * @tparam T Provider type (e.g., LVGLScope)
     * @param provider The scope provider
     */
    template <typename T>
    ComboBuilder& scope(const T& provider) {
        scope_ = provider.getScopeID();
        if constexpr (has_getIsActive_combo<T>::value) {
            isActive_ = provider.getIsActive();
        }
        return *this;
    }

    /**
     * @brief Set custom activation condition
     * @param fn Function returning true when binding should be active
     */
    ComboBuilder& when(IsActiveFn fn);

    // ═══════════════════════════════════════════════════
    // Terminal (MUST be called)
    // ═══════════════════════════════════════════════════

    /**
     * @brief Register the binding with the specified callback
     * @param cb Action to execute when both buttons are pressed
     * @return Handle for optional unbinding
     */
    BindingHandle then(ActionCallback cb);

private:
    InputBinding* registry_ = nullptr;
    ButtonID btn1_{};
    ButtonID btn2_{};
    ScopeID scope_ = 0;
    IsActiveFn isActive_ = nullptr;
    bool finalized_ = false;
};

}  // namespace oc::core::input
