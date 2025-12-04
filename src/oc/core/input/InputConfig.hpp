#pragma once

#include <cstdint>

namespace oc::core {

struct InputConfig {
    uint32_t longPressMs = 500;
    uint32_t doubleTapWindowMs = 300;
    uint32_t latchThresholdMs = 300;
    uint32_t debounceMs = 5;
};

}  // namespace oc::core
