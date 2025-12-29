#pragma once

/**
 * @file InvariantAssert.hpp
 * @brief Debug assertions for architectural invariant violations
 *
 * These macros help detect invariant violations during development.
 * They are compiled out in release builds (when NDEBUG is defined).
 *
 * @see INVARIANTS.md
 */

#include <oc/log/Log.hpp>

// ═══════════════════════════════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════════════════════════════

/**
 * Define OC_INVARIANT_BREAK to enable breakpoints on violation.
 * Useful for debugging with a connected debugger.
 */
#ifndef OC_INVARIANT_BREAK
#define OC_INVARIANT_BREAK 0
#endif

/**
 * Define OC_INVARIANT_LOG to enable logging on violation.
 * Enabled by default in debug builds.
 */
#ifndef OC_INVARIANT_LOG
#ifdef NDEBUG
#define OC_INVARIANT_LOG 0
#else
#define OC_INVARIANT_LOG 1
#endif
#endif

// ═══════════════════════════════════════════════════════════════════
// Internal Helpers
// ═══════════════════════════════════════════════════════════════════

namespace oc::debug {

/**
 * @brief Called when an invariant is violated
 *
 * Override this in your application to customize behavior.
 */
inline void onInvariantViolation(const char* invariant, const char* file, int line, const char* message) {
#if OC_INVARIANT_LOG
    OC_LOG_ERROR("[INVARIANT VIOLATION] {} at {}:{}", invariant, file, line);
    if (message && message[0] != '\0') {
        OC_LOG_ERROR("  {}", message);
    }
#endif

#if OC_INVARIANT_BREAK
    // Trigger a breakpoint (ARM Cortex-M)
    #if defined(__arm__)
    __asm volatile("bkpt #0");
    #elif defined(__x86_64__) || defined(_M_X64)
    __asm volatile("int $3");
    #endif
#endif

    (void)invariant;
    (void)file;
    (void)line;
    (void)message;
}

}  // namespace oc::debug

// ═══════════════════════════════════════════════════════════════════
// Public Macros
// ═══════════════════════════════════════════════════════════════════

#ifdef NDEBUG

// Release builds: assertions are no-ops
#define OC_ASSERT_INVARIANT(invariant, condition, message) ((void)0)
#define OC_ASSERT_SINGLE_SOURCE_OF_TRUTH(condition, message) ((void)0)
#define OC_ASSERT_INPUT_AUTHORITY(condition, message) ((void)0)
#define OC_ASSERT_HANDLER_BOUNDARY(condition, message) ((void)0)
#define OC_ASSERT_OVERLAY_LIFECYCLE(condition, message) ((void)0)

#else

/**
 * @brief Assert a general invariant
 *
 * @param invariant Name of the invariant (for logging)
 * @param condition Condition that must be true
 * @param message Human-readable message if violated
 */
#define OC_ASSERT_INVARIANT(invariant, condition, message) \
    do { \
        if (!(condition)) { \
            ::oc::debug::onInvariantViolation(invariant, __FILE__, __LINE__, message); \
        } \
    } while (0)

/**
 * @brief Assert Single Source of Truth invariant
 *
 * Use when verifying that UI reflects state correctly.
 *
 * Example:
 * @code
 * OC_ASSERT_SINGLE_SOURCE_OF_TRUTH(
 *     label_.getText() == state_.name.get(),
 *     "Label does not reflect state"
 * );
 * @endcode
 */
#define OC_ASSERT_SINGLE_SOURCE_OF_TRUTH(condition, message) \
    OC_ASSERT_INVARIANT("SingleSourceOfTruth", condition, message)

/**
 * @brief Assert Input Authority invariant
 *
 * Use when verifying that the correct scope has input authority.
 *
 * Example:
 * @code
 * OC_ASSERT_INPUT_AUTHORITY(
 *     resolver.hasAuthority(myScope),
 *     "This scope should have input authority"
 * );
 * @endcode
 */
#define OC_ASSERT_INPUT_AUTHORITY(condition, message) \
    OC_ASSERT_INVARIANT("InputAuthority", condition, message)

/**
 * @brief Assert Handler Boundary invariant
 *
 * Use when verifying handlers don't cross boundaries.
 *
 * Example: Add this in handlers to verify no LVGL calls slip through.
 */
#define OC_ASSERT_HANDLER_BOUNDARY(condition, message) \
    OC_ASSERT_INVARIANT("HandlerBoundary", condition, message)

/**
 * @brief Assert Overlay Lifecycle invariant
 *
 * Use when verifying overlay cleanup.
 *
 * Example:
 * @code
 * OC_ASSERT_OVERLAY_LIFECYCLE(
 *     !inputBinding_.isLatched(latchButton_),
 *     "Latch should be cleared on overlay close"
 * );
 * @endcode
 */
#define OC_ASSERT_OVERLAY_LIFECYCLE(condition, message) \
    OC_ASSERT_INVARIANT("OverlayLifecycle", condition, message)

#endif  // NDEBUG

// ═══════════════════════════════════════════════════════════════════
// Usage Examples
// ═══════════════════════════════════════════════════════════════════

/*

// In a handler - verify we're not calling LVGL:
void HandlerInputTransport::togglePlay() {
    // This handler should only update state, never UI
    OC_ASSERT_HANDLER_BOUNDARY(true, "Handler should not call lv_*");

    state_.transport.playing.set(!state_.transport.playing.get());
    protocol_.send(TransportPlayMessage{...});
}

// In an overlay close method:
void DeviceSelector::close() {
    inputBinding_.clearScope(scopeId_);
    inputBinding_.clearLatch(latchButton_);

    // Verify cleanup was successful
    OC_ASSERT_OVERLAY_LIFECYCLE(
        !inputBinding_.isLatched(latchButton_),
        "Latch should be cleared after close"
    );

    state_.selector.visible.set(false);
}

// In a view subscription:
void RemoteControlsView::onNameChanged(const std::string& name) {
    label_.setText(name);

    // Verify UI matches state
    OC_ASSERT_SINGLE_SOURCE_OF_TRUTH(
        name == state_.device.name.get(),
        "Label should match state after update"
    );
}

*/
