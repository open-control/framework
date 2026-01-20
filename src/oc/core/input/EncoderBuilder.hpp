#pragma once

#include <oc/core/input/BindingHandle.hpp>
#include <oc/core/input/Traits.hpp>
#include <oc/core/input/Binding.hpp>
#include <oc/types/Ids.hpp>
#include <oc/types/Callbacks.hpp>

namespace oc::core::input {

// Forward declaration
class InputBinding;

/**
 * @brief Fluent builder for encoder bindings
 *
 * Provides a type-safe, chainable API for creating encoder bindings.
 * Must call turn() gesture and terminate with then().
 *
 * Usage:
 * @code
 * onEncoder(ENC_1).turn().then([](float v){ setValue(v); });
 * onEncoder(ENC_1).turn().when(button(BTN_SHIFT).pressed()).then([](float v){ fineAdjust(v); });
 * @endcode
 */
class [[nodiscard]] EncoderBuilder {
public:
    /**
     * @brief Construct an encoder builder
     * @param registry The InputBinding that will own the binding
     * @param encoderId The encoder this binding applies to
     */
    EncoderBuilder(InputBinding* registry, EncoderID encoderId);

    /**
     * @brief Destructor warns if then() was never called
     */
    ~EncoderBuilder();

    // Move-only
    EncoderBuilder(EncoderBuilder&& other) noexcept;
    EncoderBuilder& operator=(EncoderBuilder&& other) noexcept;
    EncoderBuilder(const EncoderBuilder&) = delete;
    EncoderBuilder& operator=(const EncoderBuilder&) = delete;

    // ═══════════════════════════════════════════════════
    // Gesture selection
    // ═══════════════════════════════════════════════════

    /**
     * @brief Trigger on any encoder rotation
     *
     * For conditional triggering (e.g., while a button is pressed),
     * use when() with a predicate:
     * @code
     * onEncoder(ENC_1).turn().when(button(BTN_SHIFT).pressed()).then(...);
     * @endcode
     */
    EncoderBuilder& turn();

    // ═══════════════════════════════════════════════════
    // Modifiers (optional, chainable)
    // ═══════════════════════════════════════════════════

    /**
     * @brief Set the scope ID for this binding
     * @param s Scope identifier (non-zero for scoped bindings)
     */
    EncoderBuilder& scope(ScopeID s);

    /**
     * @brief Set scope from a duck-typed provider
     *
     * @tparam T Provider type (e.g., LVGLScope)
     * @param provider The scope provider
     */
    template <typename T>
    EncoderBuilder& scope(const T& provider) {
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
    EncoderBuilder& when(IsActiveFn fn);

    // ═══════════════════════════════════════════════════
    // Terminal (MUST be called)
    // ═══════════════════════════════════════════════════

    /**
     * @brief Register the binding with the specified callback
     * @param cb Action to execute when binding triggers (receives encoder value)
     * @return Handle for optional unbinding
     */
    BindingHandle then(EncoderActionCallback cb);

private:
    InputBinding* registry_ = nullptr;
    EncoderID encoderId_{};
    ScopeID scope_ = 0;
    IsActiveFn isActive_ = nullptr;
    bool gestureSet_ = false;
    bool finalized_ = false;
};

}  // namespace oc::core::input
