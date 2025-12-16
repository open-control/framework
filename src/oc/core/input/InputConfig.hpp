#pragma once

#include <cstdint>

#include <oc/Config.hpp>

namespace oc::core {

// Import limits from central config
using oc::config::MAX_BUTTONS;
using oc::config::MAX_ENCODERS;

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
