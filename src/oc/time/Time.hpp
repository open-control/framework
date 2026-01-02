#pragma once

/**
 * @file Time.hpp
 * @brief Platform-agnostic time provider API
 *
 * Provides a clean, generic way to access system time across the framework.
 * The HAL registers its time provider at boot via setProvider().
 *
 * Architecture:
 * - Framework defines the TimeProvider function type
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

#include <cstdint>

namespace oc::time {

/// Time provider function signature
using TimeProvider = uint32_t(*)();

/**
 * @brief Register the platform time provider
 *
 * Must be called once at boot by the HAL before any time queries.
 * If not called, millis() returns 0.
 *
 * @param provider Function returning current time in milliseconds
 */
void setProvider(TimeProvider provider);

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

}  // namespace oc::time
