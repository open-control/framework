#pragma once

#include <oc/state/Signal.hpp>

#include <cstring>
#include <functional>
#include <string>

namespace oc::state {

/**
 * @brief Zero-allocation reactive string with fixed buffer
 *
 * SignalString combines a fixed-size character buffer with a Signal to provide
 * reactive string state without dynamic allocation. Ideal for embedded systems
 * where stable pointers and predictable memory usage are required.
 *
 * ## Design Rationale
 *
 * Unlike `Signal<std::string>` which allocates on every change, SignalString:
 * - Uses a fixed buffer (no heap allocation after construction)
 * - Provides stable `const char*` pointers (safe for LVGL, etc.)
 * - Performs string comparison (strcmp) before notification
 * - Accepts both `const char*` and `std::string` inputs
 *
 * ## Usage
 *
 * @code
 * // In state struct
 * struct DeviceState {
 *     SignalString name;      // 128 chars default
 *     SignalLabel shortName;  // 32 chars for labels
 *     Signal<float> volume{0.0f};
 * };
 *
 * // Handler updates state (accepts std::string from protocol)
 * void onDeviceChange(const DeviceChangeMessage& msg) {
 *     state_.name.set(msg.deviceName);  // std::string -> buffer copy
 * }
 *
 * // View subscribes to changes
 * sub_ = state_.name.subscribe([this](const char* n) {
 *     label_.setText(n);  // const char* directly usable
 * });
 * @endcode
 *
 * ## Truncation Behavior
 *
 * If the input string exceeds buffer capacity, it is silently truncated
 * with null-termination guaranteed. The responsibility for appropriate
 * string length lies with the data source (typically the DAW/host).
 *
 * ## Why Not Signal<const char*>?
 *
 * `Signal<T>::set()` uses value comparison via `operator==`. For `const char*`,
 * this compares **pointer addresses**, not string content. SignalString handles
 * this correctly by:
 * 1. Using `strcmp()` for content comparison
 * 2. Calling `notify()` instead of `set()` on the internal Signal
 * 3. Maintaining the buffer at a fixed address
 *
 * @tparam N Buffer size in bytes (including null terminator)
 * @tparam MaxSubs Maximum concurrent subscribers (default: 4)
 *
 * @see Signal for primitive value observation
 */
template <size_t N, size_t MaxSubs = 4>
class SignalStringBase {
public:
    /// Construct with empty string
    SignalStringBase() = default;

    /// Construct with initial value
    explicit SignalStringBase(const char* initial) {
        if (initial) {
            std::strncpy(buf_, initial, N - 1);
            buf_[N - 1] = '\0';
        }
    }

    /// Construct with std::string initial value
    explicit SignalStringBase(const std::string& initial)
        : SignalStringBase(initial.c_str()) {}

    // Non-copyable, non-movable (subscribers hold pointers)
    SignalStringBase(const SignalStringBase&) = delete;
    SignalStringBase& operator=(const SignalStringBase&) = delete;
    SignalStringBase(SignalStringBase&&) = delete;
    SignalStringBase& operator=(SignalStringBase&&) = delete;

    ~SignalStringBase() = default;

    // =========================================================================
    // Setters
    // =========================================================================

    /**
     * @brief Set string value from C string
     *
     * Copies the string into the internal buffer. No notification is sent
     * if the new value equals the current value (strcmp comparison).
     *
     * @param src Source string (null-safe: nullptr is ignored)
     */
    void set(const char* src) {
        if (!src) return;
        if (std::strcmp(buf_, src) == 0) return;

        std::strncpy(buf_, src, N - 1);
        buf_[N - 1] = '\0';
        signal_.notify();
    }

    /**
     * @brief Set string value from std::string
     *
     * Convenience overload for protocol messages that use std::string.
     * Delegates to set(const char*).
     *
     * @param src Source string
     */
    void set(const std::string& src) { set(src.c_str()); }

    /**
     * @brief Force notification without changing value
     *
     * Useful for initial synchronization after subscribing.
     */
    void notify() { signal_.notify(); }

    // =========================================================================
    // Getters
    // =========================================================================

    /**
     * @brief Get current string value
     * @return Pointer to internal buffer (stable address, null-terminated)
     */
    [[nodiscard]] const char* get() const { return buf_; }

    /**
     * @brief Implicit conversion to const char*
     *
     * Allows natural usage: `label.setText(state.name);`
     */
    [[nodiscard]] operator const char*() const { return buf_; }

    /**
     * @brief Check if string is empty
     */
    [[nodiscard]] bool empty() const { return buf_[0] == '\0'; }

    /**
     * @brief Get current string length
     */
    [[nodiscard]] size_t length() const { return std::strlen(buf_); }

    // =========================================================================
    // Subscription
    // =========================================================================

    /**
     * @brief Subscribe to string changes
     *
     * @param callback Function called when string changes (receives const char*)
     * @return Subscription RAII handle (unsubscribes on destruction)
     *
     * @code
     * auto sub = state.name.subscribe([this](const char* name) {
     *     this->nameLabel_.setText(name);
     * });
     * @endcode
     */
    [[nodiscard]] Subscription subscribe(std::function<void(const char*)> callback) {
        return signal_.subscribe(std::move(callback));
    }

    /**
     * @brief Subscribe and immediately invoke callback with current value
     *
     * Useful for initializing UI state on subscription.
     *
     * @param callback Function called immediately and on future changes
     * @return Subscription RAII handle
     */
    [[nodiscard]] Subscription subscribeAndInvoke(std::function<void(const char*)> callback) {
        if (callback) {
            callback(buf_);
        }
        return subscribe(std::move(callback));
    }

    // =========================================================================
    // Capacity
    // =========================================================================

    /// Maximum string length (excluding null terminator)
    [[nodiscard]] static constexpr size_t maxLength() { return N - 1; }

    /// Buffer capacity (including null terminator)
    [[nodiscard]] static constexpr size_t capacity() { return N; }

    /// Current number of active subscribers
    [[nodiscard]] size_t subscriberCount() const { return signal_.subscriberCount(); }

    /// Maximum subscribers allowed
    [[nodiscard]] static constexpr size_t maxSubscribers() { return MaxSubs; }

private:
    char buf_[N] = {};
    Signal<const char*, MaxSubs> signal_{buf_};
};

// =============================================================================
// Predefined Aliases
// =============================================================================
// Users should prefer these aliases over specifying template parameters directly.
// The buffer sizes are chosen to cover common use cases without waste.

/**
 * @brief Default string signal (128 chars)
 *
 * Covers most device names, parameter names, track names, etc.
 * Use this unless you have specific size requirements.
 */
using SignalString = SignalStringBase<128>;

/**
 * @brief Long text signal (256 chars)
 *
 * For longer content like descriptions, paths, or formatted text.
 */
using SignalText = SignalStringBase<256>;

/**
 * @brief Short label signal (32 chars)
 *
 * For compact labels, abbreviations, or display values.
 * More memory-efficient when you know content is short.
 */
using SignalLabel = SignalStringBase<32>;

/**
 * @brief Tiny label signal (16 chars)
 *
 * For very short content like "ON", "OFF", numeric displays.
 */
using SignalTiny = SignalStringBase<16>;

}  // namespace oc::state
