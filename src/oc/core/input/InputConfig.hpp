#pragma once

#include <cstdint>

namespace oc::core {

/**
 * @brief Timing configuration for input gesture detection
 */
struct InputConfig {
    uint32_t longPressMs = 500;        ///< Long press threshold in milliseconds
    uint32_t doubleTapWindowMs = 300;  ///< Double tap detection window
    uint32_t latchThresholdMs = 300;   ///< Latch behavior threshold
    uint32_t debounceMs = 5;           ///< Button debounce time
};

}  // namespace oc::core
