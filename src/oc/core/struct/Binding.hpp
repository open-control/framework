#pragma once

#include <oc/hal/Types.hpp>

#include <cstdint>
#include <functional>
#include <optional>

namespace oc::core {

/// Predicate to check if a scoped binding should be active
using VisibilityPredicate = std::function<bool()>;

/// Unique identifier for a scope (0 = global)
using ScopeId = uintptr_t;

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
 * The isVisible predicate determines if a scoped binding is active.
 */
struct ButtonBinding {
    ButtonBindingType type;
    hal::ButtonID buttonId;
    std::optional<hal::ButtonID> secondaryButton;  ///< For COMBO
    uint32_t longPressMs = 0;                      ///< For LONG_PRESS (0 = default)
    ActionCallback action;
    bool enabled = true;
    bool latch = false;  ///< Enable latch/momentary behavior
    VisibilityPredicate isVisible = []() { return true; };
    ScopeId scopeId = 0;  ///< 0 = global
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
    VisibilityPredicate isVisible = []() { return true; };
    ScopeId scopeId = 0;  ///< 0 = global
};

}  // namespace oc::core
