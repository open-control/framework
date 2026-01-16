#pragma once

/**
 * @file Log.hpp
 * @brief Lightweight logging API with colored output and timestamps
 *
 * Platform-agnostic logging using dependency injection. The HAL provides
 * the actual output implementation via setOutput() at boot time.
 *
 * Architecture:
 * - Framework defines Output interface (struct of function pointers)
 * - HAL provides implementation (e.g., TeensyOutput with Serial)
 * - Consumer configures at boot: oc::log::setOutput(oc::hal::teensy::logOutput())
 *
 * Features:
 * - Colored output (ANSI) by log level
 * - Automatic timestamps
 * - Format string interpolation with {}
 * - Zero-cost when disabled (no OC_LOG defined)
 * - Explicit dependency injection (no linker magic)
 *
 * @code
 * // In main.cpp setup():
 * #include <oc/hal/teensy/TeensyOutput.hpp>
 * oc::log::setOutput(oc::hal::teensy::logOutput());
 *
 * // Then anywhere:
 * OC_LOG_DEBUG("Value: {}", x);     // [12ms] DEBUG: Value: 42 (cyan)
 * OC_LOG_INFO("Boot OK");           // [15ms] INFO: Boot OK    (green)
 * OC_LOG_WARN("Low: {}%", p);       // [20ms] WARN: Low: 5%    (yellow)
 * OC_LOG_ERROR("Fail: {}", e);      // [25ms] ERROR: Fail: x   (red)
 * @endcode
 */

#include <cstdint>
#include <cstddef>
#include <climits>
#include <utility>

namespace oc::log {

// =============================================================================
// Output Interface (implemented by HAL, configured via setOutput)
// =============================================================================

/**
 * @brief Output interface for logging
 *
 * HALs implement this interface to provide platform-specific output.
 * All function pointers must be non-null when passed to setOutput().
 */
struct Output {
    void (*printChar)(char);
    void (*printStr)(const char*);
    void (*printInt32)(int32_t);
    void (*printUint32)(uint32_t);
    void (*printFloat)(float);
    void (*printBool)(bool);
    uint32_t (*getTimeMs)();
};

/**
 * @brief Configure the log output implementation
 *
 * Must be called once at boot before any logging.
 * Typically called in setup() with the HAL's output implementation.
 *
 * @param output The output implementation (all pointers must be non-null)
 */
void setOutput(const Output& output);

/**
 * @brief Check if output has been configured
 * @return true if setOutput() has been called
 */
bool isConfigured();

// =============================================================================
// Internal print functions (use configured output)
// =============================================================================

void print(char c);
void print(const char* str);
void print(int32_t value);
void print(uint32_t value);
void print(float value);
void print(bool value);
uint32_t getTimeMs();

// Inline overloads for smaller/native integer types (cast to base types)
inline void print(int8_t value)   { print(static_cast<int32_t>(value)); }
inline void print(int16_t value)  { print(static_cast<int32_t>(value)); }
inline void print(uint8_t value)  { print(static_cast<uint32_t>(value)); }
inline void print(uint16_t value) { print(static_cast<uint32_t>(value)); }

// Handle native int/unsigned if different from int32_t/uint32_t
#if INT_MAX != INT32_MAX || defined(__arm__)
inline void print(int value)      { print(static_cast<int32_t>(value)); }
inline void print(unsigned value) { print(static_cast<uint32_t>(value)); }
#endif

// Handle size_t if different from uint32_t (64-bit platforms)
#if SIZE_MAX != UINT32_MAX
inline void print(size_t value) { print(static_cast<uint32_t>(value)); }
#endif

// Handle long/unsigned long - distinct types from int32_t/uint32_t on some platforms
// (e.g., Emscripten WASM, some 32-bit Unix systems)
#if LONG_MAX == INT32_MAX && !defined(__arm__)
inline void print(long value)          { print(static_cast<int32_t>(value)); }
inline void print(unsigned long value) { print(static_cast<uint32_t>(value)); }
#endif

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
// Format Implementation
// =============================================================================

namespace detail {

// Base case: print remaining string
inline void formatImpl(const char* fmt) {
    while (*fmt) {
        print(*fmt++);
    }
}

// Recursive case: find {} and replace with value
template<typename T, typename... Args>
void formatImpl(const char* fmt, T&& value, Args&&... args) {
    while (*fmt) {
        if (*fmt == '{' && *(fmt + 1) == '}') {
            print(std::forward<T>(value));
            formatImpl(fmt + 2, std::forward<Args>(args)...);
            return;
        }
        print(*fmt++);
    }
}

// Log with level prefix and color
template<typename... Args>
void log(const char* levelColor, const char* levelName, const char* fmt, Args&&... args) {
    // Timestamp
    print(color::DIM);
    print("[");
    print(getTimeMs());
    print("ms] ");
    print(color::RESET);

    // Level
    print(levelColor);
    print(levelName);
    print(": ");
    print(color::RESET);

    // Message
    formatImpl(fmt, std::forward<Args>(args)...);
    print("\n");
}

}  // namespace detail

// =============================================================================
// RAII Scope Timer
// =============================================================================

class ScopeTimer {
public:
    explicit ScopeTimer(const char* name) : name_(name), start_(getTimeMs()) {}

    ~ScopeTimer() {
        uint32_t elapsed = getTimeMs() - start_;
        print(color::DIM);
        print("[");
        print(name_);
        print("] ");
        print(elapsed);
        print("ms");
        print(color::RESET);
        print("\n");
    }

private:
    const char* name_;
    uint32_t start_;
};

}  // namespace oc::log

// =============================================================================
// Public Macros
// =============================================================================

#ifdef OC_LOG

#define OC_LOG_DEBUG(...) oc::log::detail::log(oc::log::color::CYAN,   "DEBUG", __VA_ARGS__)
#define OC_LOG_INFO(...)  oc::log::detail::log(oc::log::color::GREEN,  "INFO",  __VA_ARGS__)
#define OC_LOG_WARN(...)  oc::log::detail::log(oc::log::color::YELLOW, "WARN",  __VA_ARGS__)
#define OC_LOG_ERROR(...) oc::log::detail::log(oc::log::color::RED,    "ERROR", __VA_ARGS__)
#define OC_LOG_SCOPE(name) oc::log::ScopeTimer _oc_scope_timer_##__LINE__(name)

#else

#define OC_LOG_DEBUG(...) ((void)0)
#define OC_LOG_INFO(...)  ((void)0)
#define OC_LOG_WARN(...)  ((void)0)
#define OC_LOG_ERROR(...) ((void)0)
#define OC_LOG_SCOPE(name) ((void)0)

#endif
