#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <type_traits>
#include <utility>

#include "NotificationQueue.hpp"

namespace oc::state {

// Forward declaration
class Subscription;

namespace detail {

/// Trait to detect if T has operator==
template <typename T, typename = void>
struct has_equality : std::false_type {};

template <typename T>
struct has_equality<T, std::void_t<decltype(std::declval<T>() == std::declval<T>())>>
    : std::true_type {};

template <typename T>
inline constexpr bool has_equality_v = has_equality<T>::value;

/// Compare two values - uses operator== if available, memcmp otherwise
template <typename T>
bool equal(const T& a, const T& b) {
    if constexpr (has_equality_v<T>) {
        return a == b;
    } else {
        return std::memcmp(&a, &b, sizeof(T)) == 0;
    }
}

}  // namespace detail

/**
 * @brief Observable value that notifies subscribers on change
 *
 * Signal<T> is the core reactive primitive for state management.
 * When the value changes, all registered callbacks are invoked synchronously.
 *
 * ## Usage
 *
 * @code
 * Signal<int> counter{0};
 *
 * // Subscribe with lambda (captures supported)
 * auto sub = counter.subscribe([this](const int& val) {
 *     this->onCounterChanged(val);
 * });
 *
 * counter.set(42);  // Triggers callback immediately
 * // sub goes out of scope -> auto-unsubscribe
 * @endcode
 *
 * ## Thread Safety
 *
 * Signal is NOT thread-safe. All operations must be on the same thread.
 *
 * @tparam T Value type (must be copy-constructible)
 * @tparam MaxSubscribers Maximum number of concurrent subscribers (default: 4)
 */
template <typename T, size_t MaxSubscribers = 4>
class Signal {
public:
    /// Callback signature: receives const reference to new value
    using Callback = std::function<void(const T&)>;

    /// Default constructor - value-initializes to T{}
    Signal() : value_{} {}

    /// Construct with initial value (explicit to prevent implicit conversions)
    explicit Signal(T initial) : value_(std::move(initial)) {}

    // Non-copyable, non-movable (subscribers hold pointers to this Signal)
    Signal(const Signal&) = delete;
    Signal& operator=(const Signal&) = delete;
    Signal(Signal&&) = delete;
    Signal& operator=(Signal&&) = delete;

    ~Signal() = default;

    /// Get current value (const reference)
    [[nodiscard]] const T& get() const { return value_; }

    /// Get current value (const reference) - operator form
    [[nodiscard]] const T& operator()() const { return value_; }

    /**
     * @brief Set new value, notifying subscribers if changed
     *
     * Comparison uses operator== if available, memcmp otherwise.
     * Notifications are synchronous (callbacks invoked before set() returns).
     *
     * @param value New value
     */
    void set(const T& value) {
        if (!detail::equal(value_, value)) {
            value_ = value;
            notify();
        }
    }

    /**
     * @brief Set new value (move), notifying subscribers if changed
     * @param value New value (moved)
     */
    void set(T&& value) {
        if (!detail::equal(value_, value)) {
            value_ = std::move(value);
            notify();
        }
    }

    /**
     * @brief Enqueue notifications to all subscribers (deferred execution)
     *
     * Notifications are coalesced: if the same callback is already queued,
     * it won't be added again. This prevents duplicate updates when multiple
     * signals change in the same tick.
     *
     * Actual execution happens when NotificationQueue::flush() is called
     * (typically at the end of OpenControlApp::update()).
     */
    void notify() {
        for (size_t i = 0; i < MaxSubscribers; ++i) {
            if (callbacks_[i]) {
                // Unique key = (signal address, slot index)
                auto key = NotificationQueue::Key(static_cast<void*>(this), i);

                // Capture by value to ensure correct execution at flush time
                // The callback reads value_ at flush time (final value)
                Signal* self = this;
                size_t slot = i;

                NotificationQueue::instance().enqueue(key, [self, slot]() {
                    // Re-check callback validity (could have been unsubscribed)
                    if (self->callbacks_[slot]) {
                        self->callbacks_[slot](self->value_);
                    }
                });
            }
        }
    }

    /**
     * @brief Force immediate notification (bypass deferred queue)
     *
     * Use sparingly - defeats the purpose of coalescing.
     * Useful for critical real-time updates that can't wait.
     */
    void notifyImmediate() {
        for (auto& cb : callbacks_) {
            if (cb) {
                cb(value_);
            }
        }
    }

    /**
     * @brief Subscribe to value changes
     *
     * @param callback Function called when value changes (supports captures)
     * @return Subscription RAII handle (unsubscribes on destruction)
     *
     * @note Returns invalid Subscription if max subscribers reached
     *
     * @code
     * auto sub = signal.subscribe([this](const int& val) {
     *     this->updateDisplay(val);
     * });
     * @endcode
     */
    [[nodiscard]] Subscription subscribe(Callback callback);

    /// Current number of active subscribers
    [[nodiscard]] size_t subscriberCount() const {
        size_t count = 0;
        for (const auto& cb : callbacks_) {
            if (cb) ++count;
        }
        return count;
    }

    /// Maximum subscribers allowed
    [[nodiscard]] static constexpr size_t maxSubscribers() { return MaxSubscribers; }

private:
    friend class Subscription;

    T value_;
    std::array<Callback, MaxSubscribers> callbacks_{};

    /// Add subscriber, returns slot index or -1 if full
    int addSubscriber(Callback callback) {
        for (size_t i = 0; i < MaxSubscribers; ++i) {
            if (!callbacks_[i]) {
                callbacks_[i] = std::move(callback);
                return static_cast<int>(i);
            }
        }
        return -1;  // Full
    }

    /// Remove subscriber by slot index
    void removeSubscriber(int slot) {
        if (slot >= 0 && static_cast<size_t>(slot) < MaxSubscribers) {
            callbacks_[slot] = nullptr;
        }
    }
};

/**
 * @brief RAII subscription handle - auto-unsubscribes on destruction
 *
 * Subscriptions are move-only. When destroyed or reset, the callback is
 * automatically unregistered from the Signal.
 *
 * @code
 * class MyWidget {
 *     Subscription nameSub_;
 *
 *     void bind(Signal<std::string>& nameSignal) {
 *         nameSub_ = nameSignal.subscribe([this](const std::string& s) {
 *             this->setName(s);
 *         });
 *     }
 * };  // nameSub_ destroyed -> auto-unsubscribe
 * @endcode
 */
class Subscription {
public:
    /// Construct invalid/empty subscription
    Subscription() = default;

    /// Move constructor
    Subscription(Subscription&& other) noexcept
        : unsubscribe_(std::move(other.unsubscribe_)), slot_(other.slot_) {
        other.unsubscribe_ = nullptr;
        other.slot_ = -1;
    }

    /// Move assignment
    Subscription& operator=(Subscription&& other) noexcept {
        if (this != &other) {
            reset();
            unsubscribe_ = std::move(other.unsubscribe_);
            slot_ = other.slot_;
            other.unsubscribe_ = nullptr;
            other.slot_ = -1;
        }
        return *this;
    }

    // Non-copyable
    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;

    /// Destructor - auto-unsubscribe
    ~Subscription() { reset(); }

    /// Check if subscription is active
    [[nodiscard]] bool isValid() const { return unsubscribe_ != nullptr && slot_ >= 0; }

    /// Explicit bool conversion
    [[nodiscard]] explicit operator bool() const { return isValid(); }

    /// Manually unsubscribe (also called by destructor)
    void reset() {
        if (unsubscribe_ && slot_ >= 0) {
            unsubscribe_(slot_);
        }
        unsubscribe_ = nullptr;
        slot_ = -1;
    }

private:
    template <typename T, size_t N>
    friend class Signal;

    using UnsubscribeFn = std::function<void(int)>;

    Subscription(UnsubscribeFn unsubscribe, int slot)
        : unsubscribe_(std::move(unsubscribe)), slot_(slot) {}

    UnsubscribeFn unsubscribe_;
    int slot_ = -1;
};

// Implementation of Signal::subscribe (after Subscription is fully defined)
template <typename T, size_t MaxSubscribers>
Subscription Signal<T, MaxSubscribers>::subscribe(Callback callback) {
    if (!callback) {
        return Subscription{};
    }

    int slot = addSubscriber(std::move(callback));
    if (slot < 0) {
        // Max subscribers reached - fail loudly in debug builds
        assert(false && "Signal: MaxSubscribers exceeded. Increase template parameter.");
        return Subscription{};
    }

    // Capture 'this' to call removeSubscriber when Subscription is destroyed
    return Subscription{[this](int s) { removeSubscriber(s); }, slot};
}

}  // namespace oc::state
