#pragma once

/**
 * @file Config.hpp
 * @brief Central configuration for Open Control framework limits
 *
 * All values can be overridden via compiler flags in platformio.ini:
 *
 * @code
 * ; platformio.ini
 * build_flags =
 *     -DOC_MAX_BUTTONS=32
 *     -DOC_MAX_ENCODERS=8
 *     -DOC_MAX_CONTEXTS=8
 *     -DOC_MAX_PENDING_NOTIFICATIONS=32
 *     -DOC_MAX_SUBSCRIBERS_PER_EVENT=16
 *     -DOC_MAX_SIGNAL_SUBSCRIBERS=8
 *     -DOC_ENABLE_STATS=1
 * @endcode
 *
 * These limits affect memory usage and should be tuned per project:
 * - Smaller values = less RAM, but may hit limits
 * - Larger values = more RAM, more headroom
 */

#include <cstddef>
#include <cstdint>

namespace oc {

// ═══════════════════════════════════════════════════════════════════════════
// INPUT LIMITS
// ═══════════════════════════════════════════════════════════════════════════

/// Maximum number of physical buttons supported
#ifndef OC_MAX_BUTTONS
#define OC_MAX_BUTTONS 64
#endif
inline constexpr size_t MAX_BUTTONS = OC_MAX_BUTTONS;

/// Maximum number of physical encoders supported
#ifndef OC_MAX_ENCODERS
#define OC_MAX_ENCODERS 32
#endif
inline constexpr size_t MAX_ENCODERS = OC_MAX_ENCODERS;

// ═══════════════════════════════════════════════════════════════════════════
// BINDING LIMITS
// ═══════════════════════════════════════════════════════════════════════════

/// Maximum number of button bindings (pre-allocated)
#ifndef OC_MAX_BUTTON_BINDINGS
#define OC_MAX_BUTTON_BINDINGS 64
#endif
inline constexpr size_t MAX_BUTTON_BINDINGS = OC_MAX_BUTTON_BINDINGS;

/// Maximum number of encoder bindings (pre-allocated)
#ifndef OC_MAX_ENCODER_BINDINGS
#define OC_MAX_ENCODER_BINDINGS 32
#endif
inline constexpr size_t MAX_ENCODER_BINDINGS = OC_MAX_ENCODER_BINDINGS;

// ═══════════════════════════════════════════════════════════════════════════
// CONTEXT LIMITS
// ═══════════════════════════════════════════════════════════════════════════

/// Maximum number of contexts that can be registered
#ifndef OC_MAX_CONTEXTS
#define OC_MAX_CONTEXTS 16
#endif
inline constexpr size_t MAX_CONTEXTS = OC_MAX_CONTEXTS;

// ═══════════════════════════════════════════════════════════════════════════
// EVENT BUS LIMITS
// ═══════════════════════════════════════════════════════════════════════════

/// Maximum subscribers per event type (category+type combination)
#ifndef OC_MAX_SUBSCRIBERS_PER_EVENT
#define OC_MAX_SUBSCRIBERS_PER_EVENT 32
#endif
inline constexpr size_t MAX_SUBSCRIBERS_PER_EVENT = OC_MAX_SUBSCRIBERS_PER_EVENT;

/// Dead entries threshold before automatic compaction
#ifndef OC_EVENTBUS_COMPACT_THRESHOLD
#define OC_EVENTBUS_COMPACT_THRESHOLD 16
#endif
inline constexpr size_t EVENTBUS_COMPACT_THRESHOLD = OC_EVENTBUS_COMPACT_THRESHOLD;

// ═══════════════════════════════════════════════════════════════════════════
// NOTIFICATION QUEUE LIMITS
// ═══════════════════════════════════════════════════════════════════════════

/// Maximum pending notifications in deferred queue
#ifndef OC_MAX_PENDING_NOTIFICATIONS
#define OC_MAX_PENDING_NOTIFICATIONS 64
#endif
inline constexpr size_t MAX_PENDING_NOTIFICATIONS = OC_MAX_PENDING_NOTIFICATIONS;

// ═══════════════════════════════════════════════════════════════════════════
// SIGNAL LIMITS
// ═══════════════════════════════════════════════════════════════════════════

/// Default maximum subscribers per Signal (can be overridden per-signal via template param)
#ifndef OC_MAX_SIGNAL_SUBSCRIBERS
#define OC_MAX_SIGNAL_SUBSCRIBERS 4
#endif
inline constexpr size_t MAX_SIGNAL_SUBSCRIBERS = OC_MAX_SIGNAL_SUBSCRIBERS;

// ═══════════════════════════════════════════════════════════════════════════
// DEBUG / STATS
// ═══════════════════════════════════════════════════════════════════════════

/// Enable runtime statistics tracking (increases RAM usage slightly)
#ifndef OC_ENABLE_STATS
#define OC_ENABLE_STATS 0
#endif
inline constexpr bool ENABLE_STATS = OC_ENABLE_STATS;

// ═══════════════════════════════════════════════════════════════════════════
// MEMORY ESTIMATION HELPERS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Estimate InputBinding state memory usage
 * @return Approximate bytes for button/encoder state arrays
 */
constexpr size_t estimateInputBindingMemory() {
    // 6 arrays for button state tracking
    return MAX_BUTTONS * (sizeof(bool) * 3 + sizeof(uint32_t) * 2 + sizeof(uint8_t));
}

/**
 * @brief Estimate total framework memory footprint
 * @return Approximate bytes (excluding user contexts and HAL)
 */
constexpr size_t estimateFrameworkMemory() {
    return estimateInputBindingMemory() +
           MAX_CONTEXTS * sizeof(void*) * 2 +  // factories + names
           MAX_PENDING_NOTIFICATIONS * 32;      // Rough notification size
}

}  // namespace oc
