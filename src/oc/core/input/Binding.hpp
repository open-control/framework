#pragma once

#include <cstdint>
#include <functional>
#include <optional>

#include <oc/types/Ids.hpp>
#include <oc/types/Callbacks.hpp>

namespace oc::core::input {

// Import types from oc:: for use in this namespace
using oc::BindingID;
using oc::ScopeID;
using oc::IsActiveFn;
using oc::ActionCallback;
using oc::EncoderActionCallback;

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
 * Within the same scope, higher priority bindings are triggered first.
 * The isActive predicate determines if the binding should trigger.
 */
struct ButtonBinding {
    BindingID id = 0;                              ///< Unique ID for removal (0 = unassigned)
    ButtonBindingType type;
    ButtonID buttonId;
    std::optional<ButtonID> secondaryButton;  ///< For COMBO
    uint32_t longPressMs = 0;                      ///< For LONG_PRESS (0 = use config default)
    uint32_t doubleTapWindowMs = 0;                ///< For DOUBLE_TAP (0 = use config default)
    ActionCallback action;
    bool enabled = true;
    bool latch = false;  ///< Enable latch/momentary behavior
    IsActiveFn isActive = nullptr;  ///< Activation predicate (nullptr = always active)
    ScopeID scopeId = 0;             ///< 0 = global
    int8_t priority = 0;             ///< Higher priority = triggered first (default: 0)
};

/**
 * @brief Encoder input binding definition
 *
 * Scoped bindings (scopeId != 0) have priority over global bindings.
 * Within the same scope, higher priority bindings are triggered first.
 */
struct EncoderBinding {
    BindingID id = 0;                             ///< Unique ID for removal (0 = unassigned)
    EncoderBindingType type;
    EncoderID encoderId;
    std::optional<ButtonID> requiredButton;  ///< For TURN_WHILE_PRESSED
    EncoderActionCallback action;
    bool enabled = true;
    IsActiveFn isActive = nullptr;  ///< Activation predicate (nullptr = always active)
    ScopeID scopeId = 0;            ///< 0 = global
    int8_t priority = 0;            ///< Higher priority = triggered first (default: 0)
};

}  // namespace oc::core::input
