#pragma once

#include <cstdint>

#include <oc/Config.hpp>

namespace oc::core::input {

/**
 * @brief Strict physical-button gesture contract
 *
 * GestureRoutingPolicy::PressScoped is defined by eight fail-closed rules:
 * 1. Capture the current input authority when the physical press starts.
 * 2. After the synchronous press callback establishes any same-scope inline
 *    mode, freeze release/long-press/double-tap/combo on the captured route.
 * 3. Recheck only the captured binding's predicate; never search a replacement.
 * 4. Quarantine held buttons when overlay authority actually changes,
 *    including a transition initiated through the underlying visibility stack.
 * 5. Retarget an in-flight gesture only through explicit handoffPress().
 * 6. Consume an unbound gesture owned by an active scope; never fall through.
 * 7. Consume equal-highest-priority ambiguities instead of choosing by order.
 * 8. Admit globals through active scopes only when marked globalPassThrough().
 *
 * These rules cover multi-phase physical button gestures. Encoder turns are
 * instantaneous events and retain their existing per-event authority routing.
 * See INPUT_ROUTING_POLICY.md at the framework root for binding guidance.
 */

/**
 * @brief Policy for routing button release events after scoped press ownership
 */
enum class ReleaseRoutingPolicy : uint8_t {
    /**
     * @brief Try press owner first, then fall back to normal scoped/global release dispatch
     *
     * This preserves the existing release routing contract.
     */
    OwnerThenFallback,

    /**
     * @brief Try press owner only, consume if no owner release handler matches
     *
     * This prevents cross-scope release side effects when overlay authority
     * changes between PRESS and RELEASE.
     */
    OwnerOnly,
};

/**
 * @brief Policy used to bind the phases of one physical button gesture
 */
enum class GestureRoutingPolicy : uint8_t {
    /**
     * @brief Resolve every phase with the historical dispatch rules
     */
    Legacy,

    /**
     * @brief Capture one route on press and keep it until release
     *
     * Authority is selected before Press dispatch. Remaining phase handlers
     * are frozen after that synchronous Press callback, so it may establish a
     * same-scope inline mode without permitting later retargeting.
     */
    PressScoped,
};

/**
 * @brief Policy for multiple active bindings at the same highest priority
 */
enum class BindingAmbiguityPolicy : uint8_t {
    FirstMatch,
    FailClosed,
};

/**
 * @brief Policy for global bindings while strict gesture routing is enabled
 */
enum class GlobalRoutingPolicy : uint8_t {
    /**
     * @brief Preserve normal global fallback behavior
     */
    Fallback,

    /**
     * @brief Only explicitly declared pass-through bindings may route globally
     */
    ExplicitPassThroughOnly,
};

// Import limits from central config
using oc::MAX_BUTTONS;
using oc::MAX_ENCODERS;

/**
 * @brief Timing configuration for input gesture detection
 */
struct InputConfig {
    uint32_t longPressMs = 500;        ///< Long press threshold in milliseconds
    uint32_t doubleTapWindowMs = 300;  ///< Double tap detection window
    uint32_t latchThresholdMs = 300;   ///< Latch behavior threshold
    uint32_t debounceMs = 5;           ///< Button debounce time
    ReleaseRoutingPolicy releaseRoutingPolicy = ReleaseRoutingPolicy::OwnerThenFallback;
    GestureRoutingPolicy gestureRoutingPolicy = GestureRoutingPolicy::Legacy;
    BindingAmbiguityPolicy ambiguityPolicy = BindingAmbiguityPolicy::FirstMatch;
    GlobalRoutingPolicy globalRoutingPolicy = GlobalRoutingPolicy::Fallback;
};

}  // namespace oc::core::input
