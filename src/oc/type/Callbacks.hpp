#pragma once

/**
 * @file Callbacks.hpp
 * @brief Callback type aliases (Level 0 - minimal dependencies)
 */

#include <cstdint>
#include <functional>

#include "Ids.hpp"

namespace oc::type {

// ═══════════════════════════════════════════════════════════════════════════
// Time provider
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Function pointer for getting current time in milliseconds
 *
 * Injected at app construction time, allows HAL-specific time sources.
 */
using TimeProvider = uint32_t (*)();

// ═══════════════════════════════════════════════════════════════════════════
// Button types
// ═══════════════════════════════════════════════════════════════════════════

enum class ButtonEvent : uint8_t {
    PRESSED,
    RELEASED
};

using ButtonCallback = std::function<void(ButtonID, ButtonEvent)>;

// ═══════════════════════════════════════════════════════════════════════════
// Encoder types
// ═══════════════════════════════════════════════════════════════════════════

using EncoderCallback = std::function<void(EncoderID id, float value)>;

// ═══════════════════════════════════════════════════════════════════════════
// Generic callbacks
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Simple action callback (no parameters)
 */
using ActionCallback = std::function<void()>;

/**
 * @brief Encoder action callback with delta value
 */
using EncoderActionCallback = std::function<void(float)>;

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

}  // namespace oc::type
