#pragma once

/**
 * @file Warning.hpp
 * @brief Platform-agnostic warning handler for runtime validation
 *
 * This module provides a lightweight, zero-overhead warning system
 * that can be optionally configured by the consumer. When no handler
 * is set, warnings are silently ignored.
 *
 * Usage:
 * @code
 * // In consumer's main.cpp setup()
 * oc::core::setWarningHandler([](const char* msg) {
 *     Serial.println(msg);
 * });
 * @endcode
 */

namespace oc::core {

/**
 * @brief Warning handler function signature
 * @param message Null-terminated warning message
 */
using WarningHandler = void (*)(const char* message);

/**
 * @brief Global warning handler (inline for header-only)
 *
 * Default is nullptr (warnings silently ignored).
 * Set via setWarningHandler() during application setup.
 */
inline WarningHandler g_warningHandler = nullptr;

/**
 * @brief Set the global warning handler
 *
 * @param handler Function to call for warnings, or nullptr to disable
 *
 * Example:
 * @code
 * setWarningHandler([](const char* msg) {
 *     Serial.print("[WARN] ");
 *     Serial.println(msg);
 * });
 * @endcode
 */
inline void setWarningHandler(WarningHandler handler) {
    g_warningHandler = handler;
}

/**
 * @brief Emit a warning message
 *
 * If no handler is set, this is a no-op.
 * Zero overhead when g_warningHandler is nullptr.
 *
 * @param message Warning message to emit
 */
inline void warn(const char* message) {
    if (g_warningHandler) {
        g_warningHandler(message);
    }
}

}  // namespace oc::core
