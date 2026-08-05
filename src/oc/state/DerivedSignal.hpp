#pragma once

/**
 * @file DerivedSignal.hpp
 * @brief Derived signals that automatically update from source signals
 *
 * DerivedSignal transforms a source signal's value through a function
 * and exposes the result as a subscribable signal. Updates are automatic
 * when the source changes.
 *
 * DerivedStringSignal is specialized for string outputs with fixed buffers
 * (no heap allocation).
 */

#include <cstdint>
#include <cstdio>
#include <functional>

#include "Signal.hpp"
#include "SignalString.hpp"

namespace oc::state {

/**
 * @brief Signal whose value is derived from another signal
 *
 * Automatically updates when source signal changes.
 * The output is a Signal<TOut> that can be subscribed to.
 *
 * @tparam TIn Source signal value type
 * @tparam TOut Output signal value type
 * @tparam MaxSubscribers Maximum subscribers for output signal
 *
 * @code
 * Signal<int> source{10};
 * DerivedSignal<int, float> derived{source, [](int v) {
 *     return v / 100.0f;
 * }};
 * // derived.get() == 0.1f
 * // Updates automatically when source changes
 * @endcode
 */
template <typename TIn, typename TOut, size_t MaxSubscribers = 4>
class DerivedSignal {
public:
    /// Transform function signature: TIn -> TOut
    using TransformFn = std::function<TOut(const TIn&)>;

    /**
     * @brief Construct derived signal from source with transform function
     * @param source Source signal to derive from
     * @param transform Function to transform source value to output value
     */
    DerivedSignal(Signal<TIn>& source, TransformFn transform)
        : transform_(std::move(transform))
        , output_(transform_(source.get()))
        , subscription_(source.subscribe([this](const TIn& v) {
              output_.set(transform_(v));
          })) {}

    // Non-copyable, non-movable (subscribers hold pointers)
    DerivedSignal(const DerivedSignal&) = delete;
    DerivedSignal& operator=(const DerivedSignal&) = delete;
    DerivedSignal(DerivedSignal&&) = delete;
    DerivedSignal& operator=(DerivedSignal&&) = delete;

    ~DerivedSignal() = default;

    /// Get current derived value
    [[nodiscard]] const TOut& get() const { return output_.get(); }

    /// Get current derived value (operator form)
    [[nodiscard]] const TOut& operator()() const { return output_.get(); }

    /**
     * @brief Subscribe to derived value changes
     * @param callback Function called when derived value changes
     * @return Subscription RAII handle
     */
    [[nodiscard]] Subscription subscribe(
        typename Signal<TOut, MaxSubscribers>::Callback callback
    ) const {
        return output_.subscribe(std::move(callback));
    }

    /// Current number of active subscribers
    [[nodiscard]] size_t subscriberCount() const { return output_.subscriberCount(); }

    /// Maximum subscribers allowed
    [[nodiscard]] static constexpr size_t maxSubscribers() { return MaxSubscribers; }

private:
    TransformFn transform_;
    Signal<TOut, MaxSubscribers> output_;
    Subscription subscription_;  ///< Subscription to source signal
};

/**
 * @brief Signal whose string value is derived from another signal
 *
 * Specialized for string outputs with fixed-size buffer (no heap allocation).
 * The transform function writes directly to the output buffer.
 *
 * @tparam TIn Source signal value type
 * @tparam MaxLen Maximum string length (including null terminator)
 * @tparam MaxSubscribers Maximum subscribers for output signal
 *
 * @code
 * Signal<float> value{0.5f};
 * DerivedStringSignal<float, 8> display{value, [](float v, char* buf, size_t size) {
 *     uint8_t cc = static_cast<uint8_t>(v * 127.0f);
 *     snprintf(buf, size, "%d", cc);
 * }};
 * // display.get() == "64"
 * // Updates automatically when value changes
 * @endcode
 */
template <typename TIn, size_t MaxLen = 32, size_t MaxSubscribers = 2>
class DerivedStringSignal {
public:
    /// Transform function signature: writes to buffer
    using TransformFn = std::function<void(const TIn&, char*, size_t)>;

    /**
     * @brief Construct derived string signal from source with transform function
     * @param source Source signal to derive from
     * @param transform Function to transform source value to string in buffer
     */
    DerivedStringSignal(Signal<TIn>& source, TransformFn transform)
        : transform_(std::move(transform))
        , subscription_(source.subscribe([this](const TIn& v) { updateOutput(v); })) {
        // Initialize with current source value
        updateOutput(source.get());
    }

    // Non-copyable, non-movable (subscribers hold pointers)
    DerivedStringSignal(const DerivedStringSignal&) = delete;
    DerivedStringSignal& operator=(const DerivedStringSignal&) = delete;
    DerivedStringSignal(DerivedStringSignal&&) = delete;
    DerivedStringSignal& operator=(DerivedStringSignal&&) = delete;

    ~DerivedStringSignal() = default;

    /// Get current string value
    [[nodiscard]] const char* get() const { return output_.get(); }

    /// Implicit conversion to const char*
    [[nodiscard]] operator const char*() const { return output_.get(); }

    /**
     * @brief Subscribe to string value changes
     * @param callback Function called when string changes
     * @return Subscription RAII handle
     */
    [[nodiscard]] Subscription subscribe(
        std::function<void(const char*)> callback
    ) const {
        return output_.subscribe(std::move(callback));
    }

#if OC_ENABLE_STATS
    /** Attach a semantic label to the derived output for queue diagnostics. */
    void setDebugLabel(const char* label) { output_.setDebugLabel(label); }

    /** Return the derived output's diagnostic label, or nullptr if unset. */
    [[nodiscard]] const char* debugLabel() const { return output_.debugLabel(); }
#endif

    /// Current number of active subscribers
    [[nodiscard]] size_t subscriberCount() const { return output_.subscriberCount(); }

    /// Maximum string length (excluding null terminator)
    [[nodiscard]] static constexpr size_t maxLength() { return MaxLen - 1; }

    /// Maximum subscribers allowed
    [[nodiscard]] static constexpr size_t maxSubscribers() { return MaxSubscribers; }

private:
    void updateOutput(const TIn& value) {
        char buf[MaxLen];
        transform_(value, buf, MaxLen);
        output_.set(buf);
    }

    TransformFn transform_;
    SignalStringBase<MaxLen, MaxSubscribers> output_;
    Subscription subscription_;  ///< Subscription to source signal
};

}  // namespace oc::state
