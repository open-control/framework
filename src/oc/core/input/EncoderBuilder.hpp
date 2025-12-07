#pragma once

#include <type_traits>
#include <utility>

#include <oc/core/Warning.hpp>
#include <oc/core/input/BindingHandle.hpp>
#include <oc/core/struct/Binding.hpp>
#include <oc/hal/Types.hpp>

namespace oc::core::input {

// Forward declaration
class InputBinding;

// SFINAE helper (same as in ButtonBuilder)
template <typename T, typename = void>
struct has_getIsActive_encoder : std::false_type {};

template <typename T>
struct has_getIsActive_encoder<T, std::void_t<decltype(std::declval<const T&>().getIsActive())>>
    : std::true_type {};

/**
 * @brief Fluent builder for encoder bindings
 *
 * Provides a type-safe, chainable API for creating encoder bindings.
 * Must call a gesture method (onTurn, onTurnWhilePressed) and terminate with then().
 *
 * Usage:
 * @code
 * api.encoder(ENC_1).onTurn().then([](float v){ setValue(v); });
 * api.encoder(ENC_1).onTurnWhilePressed(BTN_SHIFT).scope(s).then([](float v){ fineAdjust(v); });
 * @endcode
 */
class [[nodiscard]] EncoderBuilder {
public:
    /**
     * @brief Construct an encoder builder
     * @param registry The InputBinding that will own the binding
     * @param encoderId The encoder this binding applies to
     */
    EncoderBuilder(InputBinding* registry, hal::EncoderID encoderId);

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
    // Gesture selection (exactly ONE required)
    // ═══════════════════════════════════════════════════

    /**
     * @brief Trigger on any encoder rotation
     */
    EncoderBuilder& onTurn();

    /**
     * @brief Trigger only when a button is held while turning
     * @param btn The button that must be pressed
     */
    EncoderBuilder& onTurnWhilePressed(hal::ButtonID btn);

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
        if constexpr (has_getIsActive_encoder<T>::value) {
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
    hal::EncoderID encoderId_{};
    EncoderBindingType type_ = EncoderBindingType::TURN;
    std::optional<hal::ButtonID> requiredButton_ = std::nullopt;
    ScopeID scope_ = 0;
    IsActiveFn isActive_ = nullptr;
    bool gestureSet_ = false;
    bool finalized_ = false;
};

}  // namespace oc::core::input
