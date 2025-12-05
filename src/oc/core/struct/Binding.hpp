#pragma once

#include <cstdint>
#include <functional>
#include <optional>

#include <oc/hal/Types.hpp>

namespace oc::core {

/**
 * @brief Predicate for conditional binding activation
 *
 * Called each time an input event matches this binding.
 * Return true to trigger the action, false to skip.
 *
 * Use cases:
 * - UI frameworks: only trigger when a widget/view is visible
 * - State machines: only trigger in certain application states
 * - Modal dialogs: disable background bindings while modal is open
 *
 * If nullptr, binding is always active when in scope.
 */
using IsActiveFn = std::function<bool()>;

/**
 * @brief Unique scope identifier for binding grouping
 *
 * Purposes:
 * 1. Batch removal: clearScope(id) removes all bindings with this scope
 * 2. Priority: scoped bindings trigger before global (scope=0) bindings
 *
 * Typical values:
 * - 0: Global binding (no scope, lowest priority)
 * - Pointer cast to uintptr_t (e.g., view/screen instance)
 * - Enum value for application modes
 */
using ScopeID = uintptr_t;

using ActionCallback = std::function<void()>;
using EncoderActionCallback = std::function<void(float)>;

/**
 * @brief Button binding trigger types
 */
enum class ButtonBindingType : uint8_t {
    PRESS,       ///< Triggered on button press
    RELEASE,     ///< Triggered on button release
    LONG_PRESS,  ///< Triggered after hold duration
    DOUBLE_TAP,  ///< Triggered on rapid double press
    COMBO        ///< Triggered when two buttons pressed together
};

/**
 * @brief Encoder binding trigger types
 */
enum class EncoderBindingType : uint8_t {
    TURN,               ///< Triggered on any encoder rotation
    TURN_WHILE_PRESSED  ///< Triggered only when a button is held
};

/**
 * @brief Button input binding definition
 *
 * Scoped bindings (scopeId != 0) have priority over global bindings.
 * The isActive predicate determines if the binding should trigger.
 */
struct ButtonBinding {
    ButtonBindingType type;
    hal::ButtonID buttonId;
    std::optional<hal::ButtonID> secondaryButton;  ///< For COMBO
    uint32_t longPressMs = 0;                      ///< For LONG_PRESS (0 = use config default)
    uint32_t doubleTapWindowMs = 0;                ///< For DOUBLE_TAP (0 = use config default)
    ActionCallback action;
    bool enabled = true;
    bool latch = false;  ///< Enable latch/momentary behavior
    IsActiveFn isActive = nullptr;  ///< Activation predicate (nullptr = always active)
    ScopeID scopeId = 0;             ///< 0 = global
};

/**
 * @brief Encoder input binding definition
 */
struct EncoderBinding {
    EncoderBindingType type;
    hal::EncoderID encoderId;
    std::optional<hal::ButtonID> requiredButton;  ///< For TURN_WHILE_PRESSED
    EncoderActionCallback action;
    bool enabled = true;
    IsActiveFn isActive = nullptr;  ///< Activation predicate (nullptr = always active)
    ScopeID scopeId = 0;            ///< 0 = global
};

}  // namespace oc::core
