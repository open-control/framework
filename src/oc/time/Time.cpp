/**
 * @file Time.cpp
 * @brief Time provider implementation
 */

#include <oc/time/Time.hpp>

namespace oc::time {

namespace {

/// Default no-op provider (returns 0)
uint32_t noopProvider() { return 0; }

/// Current provider (initialized to no-op)
oc::type::TimeProvider g_provider = noopProvider;

/// Configuration flag
bool g_configured = false;

}  // namespace

void setProvider(oc::type::TimeProvider provider) {
    if (provider) {
        g_provider = provider;
        g_configured = true;
    }
}

uint32_t millis() {
    return g_provider();
}

bool isConfigured() {
    return g_configured;
}

}  // namespace oc::time
