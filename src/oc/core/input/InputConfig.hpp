#pragma once

#include <cstdint>

#include <oc/Config.hpp>

namespace oc::core::input {

/**
 * @brief Policy for routing button release events after scoped press ownership
 */
enum class ReleaseRoutingPolicy : uint8_t {
    /**
     * @brief Try press owner first, then fall back to normal scoped/global release dispatch
     *
     * This preserves legacy behavior and backward compatibility.
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
};

}  // namespace oc::core::input
