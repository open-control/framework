#pragma once

/**
 * @file Time.hpp
 * @brief Platform-agnostic time provider API
 *
 * Provides application and realtime time providers.
 *
 * Architecture:
 * - Framework defines the oc::type::TimeProvider function type
 * - HAL provides implementation (e.g., Arduino millis())
 * - Consumer calls oc::time::millis() anywhere
 *
 * @code
 * // In HAL init (e.g., TeensyApp):
 * oc::time::setProvider([]() { return ::millis(); });
 *
 * // Then anywhere in application code:
 * uint32_t now = oc::time::millis();
 * if (now - lastUpdate >= 100) { ... }
 * @endcode
 */

#include <oc/type/Callbacks.hpp>

namespace oc::time {

// Use oc::type::TimeProvider from oc::type (avoid duplication)

/**
 * @brief Register the platform time provider
 *
 * Must be called once at boot by the HAL before any time queries.
 * If not called, millis() returns 0.
 *
 * @param provider Function returning current time in milliseconds
 */
void setProvider(oc::type::TimeProvider provider);

/**
 * @brief Get current time in milliseconds
 *
 * Returns the time from the registered provider, or 0 if none registered.
 *
 * @return Current time in milliseconds
 */
uint32_t millis();

/**
 * @brief Check if a time provider has been registered
 * @return true if setProvider() has been called
 */
bool isConfigured();

/// Register the realtime microsecond provider.
void setMicrosProvider(oc::type::MicrosProvider provider);

/// Current realtime in microseconds. Use signedDeltaUs() for deadline checks.
uint32_t micros32();

/// True when a realtime microsecond provider is available.
bool isMicrosConfigured();

/// Positive: late. Zero: on time. Negative: early.
inline int32_t signedDeltaUs(uint32_t nowUs, uint32_t deadlineUs) {
    return static_cast<int32_t>(nowUs - deadlineUs);
}

}  // namespace oc::time
