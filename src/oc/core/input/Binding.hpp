#pragma once

#include <cstdint>
#include <functional>
#include <optional>

#include <oc/type/Ids.hpp>
#include <oc/type/Callbacks.hpp>

namespace oc::core::input {

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
    oc::type::BindingID id = 0;                    ///< Unique ID for removal (0 = unassigned)
    ButtonBindingType type;
    oc::type::ButtonID buttonId;
    std::optional<oc::type::ButtonID> secondaryButton;  ///< For COMBO
    uint32_t longPressMs = 0;                      ///< For LONG_PRESS (0 = use config default)
    uint32_t doubleTapWindowMs = 0;                ///< For DOUBLE_TAP (0 = use config default)
    oc::type::ActionCallback action;
    bool enabled = true;
    bool latch = false;  ///< Enable latch/momentary behavior
    bool globalPassThrough = false;  ///< Reserved global control allowed through active scopes
    oc::type::IsActiveFn isActive = nullptr;  ///< Activation predicate (nullptr = always active)
    oc::type::ScopeID scopeId = 0;            ///< 0 = global
    int8_t priority = 0;                      ///< Higher priority = triggered first (default: 0)
};

/**
 * @brief Encoder input binding definition
 *
 * Scoped bindings (scopeId != 0) have priority over global bindings.
 * Within the same scope, higher priority bindings are triggered first.
 */
struct EncoderBinding {
    oc::type::BindingID id = 0;                   ///< Unique ID for removal (0 = unassigned)
    EncoderBindingType type;
    oc::type::EncoderID encoderId;
    std::optional<oc::type::ButtonID> requiredButton;  ///< For TURN_WHILE_PRESSED
    oc::type::EncoderActionCallback action;
    bool enabled = true;
    oc::type::IsActiveFn isActive = nullptr;  ///< Activation predicate (nullptr = always active)
    oc::type::ScopeID scopeId = 0;            ///< 0 = global
    int8_t priority = 0;                      ///< Higher priority = triggered first (default: 0)
};

}  // namespace oc::core::input
