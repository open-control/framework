/**
 * @file Time.cpp
 * @brief Time provider implementation
 */

#include <oc/time/Time.hpp>

#ifndef ARDUINO
    #include <chrono>
#endif

namespace oc::time {

namespace {

/// Default no-op provider (returns 0)
uint32_t noopProvider() { return 0; }

#ifndef ARDUINO
uint32_t steadyMicrosProvider() {
    using namespace std::chrono;
    return static_cast<uint32_t>(
        duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count()
    );
}
#endif

/// Current provider (initialized to no-op)
oc::type::TimeProvider g_provider = noopProvider;
#ifdef ARDUINO
oc::type::MicrosProvider g_micros_provider = noopProvider;
bool g_micros_configured = false;
#else
oc::type::MicrosProvider g_micros_provider = steadyMicrosProvider;
bool g_micros_configured = true;
#endif

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

void setMicrosProvider(oc::type::MicrosProvider provider) {
    if (provider) {
        g_micros_provider = provider;
        g_micros_configured = true;
    }
}

uint32_t micros32() {
    return g_micros_provider();
}

bool isMicrosConfigured() {
    return g_micros_configured;
}

}  // namespace oc::time
