#pragma once

/**
 * @file Log.hpp
 * @brief Lightweight logging API with colored output and timestamps
 *
 * Features:
 * - Colored output (ANSI) by log level
 * - Automatic timestamps (uses framework TimeProvider)
 * - Format string interpolation with {}
 * - Zero-cost when disabled (OC_LOG_DISABLED)
 * - No-op fallback when no HAL (OC_LOG_PRINT undefined)
 *
 * @code
 * OC_LOG_DEBUG("Value: {}", x);     // [12ms] DEBUG: Value: 42 (cyan)
 * OC_LOG_INFO("Boot OK");           // [15ms] INFO: Boot OK    (green)
 * OC_LOG_WARN("Low: {}%", p);       // [20ms] WARN: Low: 5%    (yellow)
 * OC_LOG_ERROR("Fail: {}", e);      // [25ms] ERROR: Fail: x   (red)
 *
 * { OC_LOG_SCOPE("init"); code(); } // [init] 45ms
 * @endcode
 */

#include <cstdint>
#include <type_traits>

#include <oc/hal/Types.hpp>

namespace oc::log {

// =============================================================================
// ANSI Color Codes
// =============================================================================

namespace color {
constexpr const char* RESET  = "\033[0m";
constexpr const char* CYAN   = "\033[36m";
constexpr const char* GREEN  = "\033[32m";
constexpr const char* YELLOW = "\033[33m";
constexpr const char* RED    = "\033[31m";
constexpr const char* DIM    = "\033[2m";
}  // namespace color

// =============================================================================
// Time Provider (set by AppBuilder)
// =============================================================================

inline hal::TimeProvider timeProvider_ = nullptr;

inline void setTimeProvider(hal::TimeProvider tp) {
    timeProvider_ = tp;
}

inline uint32_t getTime() {
    return timeProvider_ ? timeProvider_() : 0;
}

// =============================================================================
// Format Implementation
// =============================================================================

namespace detail {

// Base case: print remaining string
inline void formatImpl(const char* fmt) {
#ifdef OC_LOG_PRINT
    while (*fmt) {
        OC_LOG_PRINT(*fmt++);
    }
#else
    (void)fmt;
#endif
}

// Recursive case: find {} and replace with value
template<typename T, typename... Args>
void formatImpl(const char* fmt, T&& value, Args&&... args) {
#ifdef OC_LOG_PRINT
    while (*fmt) {
        if (*fmt == '{' && *(fmt + 1) == '}') {
            // Print the value
            OC_LOG_PRINT(value);
            // Continue with rest of format string
            formatImpl(fmt + 2, std::forward<Args>(args)...);
            return;
        }
        OC_LOG_PRINT(*fmt++);
    }
#else
    (void)fmt;
    (void)value;
    ((void)args, ...);
#endif
}

// Log with level prefix and color
template<typename... Args>
void log(const char* levelColor, const char* levelName, const char* fmt, Args&&... args) {
#ifdef OC_LOG_PRINT
    // Timestamp
    OC_LOG_PRINT(color::DIM);
    OC_LOG_PRINT("[");
    OC_LOG_PRINT(getTime());
    OC_LOG_PRINT("ms] ");
    OC_LOG_PRINT(color::RESET);

    // Level
    OC_LOG_PRINT(levelColor);
    OC_LOG_PRINT(levelName);
    OC_LOG_PRINT(": ");
    OC_LOG_PRINT(color::RESET);

    // Message
    formatImpl(fmt, std::forward<Args>(args)...);
    OC_LOG_PRINT("\n");
#else
    (void)levelColor;
    (void)levelName;
    (void)fmt;
    ((void)args, ...);
#endif
}

}  // namespace detail

// =============================================================================
// RAII Scope Timer
// =============================================================================

class ScopeTimer {
public:
    explicit ScopeTimer(const char* name) : name_(name), start_(getTime()) {}

    ~ScopeTimer() {
#ifdef OC_LOG_PRINT
        uint32_t elapsed = getTime() - start_;
        OC_LOG_PRINT(color::DIM);
        OC_LOG_PRINT("[");
        OC_LOG_PRINT(name_);
        OC_LOG_PRINT("] ");
        OC_LOG_PRINT(elapsed);
        OC_LOG_PRINT("ms");
        OC_LOG_PRINT(color::RESET);
        OC_LOG_PRINT("\n");
#endif
    }

private:
    const char* name_;
    uint32_t start_;
};

}  // namespace oc::log

// =============================================================================
// Public Macros
// =============================================================================

#if defined(OC_LOG_DISABLED)

// Disabled: all macros become no-ops
#define OC_LOG_DEBUG(...) ((void)0)
#define OC_LOG_INFO(...)  ((void)0)
#define OC_LOG_WARN(...)  ((void)0)
#define OC_LOG_ERROR(...) ((void)0)
#define OC_LOG_SCOPE(name) ((void)0)

#else

// Enabled: actual implementation
#define OC_LOG_DEBUG(...) oc::log::detail::log(oc::log::color::CYAN,   "DEBUG", __VA_ARGS__)
#define OC_LOG_INFO(...)  oc::log::detail::log(oc::log::color::GREEN,  "INFO",  __VA_ARGS__)
#define OC_LOG_WARN(...)  oc::log::detail::log(oc::log::color::YELLOW, "WARN",  __VA_ARGS__)
#define OC_LOG_ERROR(...) oc::log::detail::log(oc::log::color::RED,    "ERROR", __VA_ARGS__)
#define OC_LOG_SCOPE(name) oc::log::ScopeTimer _oc_scope_timer_##__LINE__(name)

#endif
