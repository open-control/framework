#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

#include <oc/log/Log.hpp>

#include "NotificationQueue.hpp"

namespace oc::state {

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
        static_assert(std::is_trivially_copyable_v<T>,
            "Signal<T>: T must have operator== or be trivially copyable for memcmp comparison");
        return std::memcmp(&a, &b, sizeof(T)) == 0;
    }
}

struct SubscriptionDebugContext {
    const char* requesterLabel = nullptr;
};

inline const SubscriptionDebugContext*& currentSubscriptionDebugContext() {
    static const SubscriptionDebugContext* context = nullptr;
    return context;
}

class ScopedSubscriptionDebugContext {
public:
    explicit ScopedSubscriptionDebugContext(const char* requesterLabel)
        : previous_(currentSubscriptionDebugContext()) {
        context_.requesterLabel = requesterLabel;
        currentSubscriptionDebugContext() = &context_;
    }

    ScopedSubscriptionDebugContext(const ScopedSubscriptionDebugContext&) = delete;
    ScopedSubscriptionDebugContext& operator=(const ScopedSubscriptionDebugContext&) = delete;

    ~ScopedSubscriptionDebugContext() {
        currentSubscriptionDebugContext() = previous_;
    }

private:
    SubscriptionDebugContext context_{};
    const SubscriptionDebugContext* previous_ = nullptr;
};

}  // namespace detail

/**
 * @brief Observable value that notifies subscribers on change
 *
 * Signal<T> is the core reactive primitive for state management.
 * When the value changes, notifications are queued and executed when
 * NotificationQueue::flush() is called (typically at end of update loop).
 * For immediate execution, use notifyImmediate().
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

    /**
     * @brief Attach an optional debug label used in overflow diagnostics
     *
     * This is intended for high-fan-out UI signals where subscriber overflow
     * is otherwise hard to localize from a crash log alone.
     */
    void setDebugLabel(const char* label) { debug_label_ = label; }

    /**
     * @brief Return the debug label, or nullptr if unset
     */
    [[nodiscard]] const char* debugLabel() const { return debug_label_; }

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
    const char* debug_label_ = nullptr;

    void reportSubscriberOverflow_() const {
        const auto* context = detail::currentSubscriptionDebugContext();
        OC_LOG_ERROR(
            "[Signal] MaxSubscribers exceeded label={} subscribers={} max={} requester={} address={}",
            debug_label_ ? debug_label_ : "<unnamed>",
            subscriberCount(),
            MaxSubscribers,
            (context && context->requesterLabel) ? context->requesterLabel : "<unknown>",
            static_cast<const void*>(this)
        );
    }

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
        : owner_(other.owner_), unsubscribe_(other.unsubscribe_), slot_(other.slot_) {
        other.owner_ = nullptr;
        other.unsubscribe_ = nullptr;
        other.slot_ = -1;
    }

    /// Move assignment
    Subscription& operator=(Subscription&& other) noexcept {
        if (this != &other) {
            reset();
            owner_ = other.owner_;
            unsubscribe_ = other.unsubscribe_;
            slot_ = other.slot_;
            other.owner_ = nullptr;
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
    [[nodiscard]] bool isValid() const {
        return owner_ != nullptr && unsubscribe_ != nullptr && slot_ >= 0;
    }

    /// Explicit bool conversion
    [[nodiscard]] explicit operator bool() const { return isValid(); }

    /// Manually unsubscribe (also called by destructor)
    void reset() {
        if (unsubscribe_ && owner_ && slot_ >= 0) {
            unsubscribe_(owner_, slot_);
        }
        owner_ = nullptr;
        unsubscribe_ = nullptr;
        slot_ = -1;
    }

private:
    template <typename T, size_t N>
    friend class Signal;

    using UnsubscribeFn = void (*)(void*, int);

    Subscription(void* owner, UnsubscribeFn unsubscribe, int slot)
        : owner_(owner), unsubscribe_(unsubscribe), slot_(slot) {}

    void* owner_ = nullptr;
    UnsubscribeFn unsubscribe_ = nullptr;
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
        reportSubscriberOverflow_();
        assert(false && "Signal: MaxSubscribers exceeded. Check logs for label/subscriber count.");
        return Subscription{};
    }

    // Capture 'this' to call removeSubscriber when Subscription is destroyed
    return Subscription{
        static_cast<void*>(this),
        [](void* owner, int s) {
            static_cast<Signal*>(owner)->removeSubscriber(s);
        },
        slot
    };
}

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Convert array of Signals to vector of values
 *
 * Useful when UI components need a snapshot of multiple reactive values.
 * Use sparingly in embedded contexts (allocates memory).
 *
 * @tparam T Value type
 * @tparam N Array size
 * @tparam MaxSubs Max subscribers per signal
 * @param signals Array of Signal<T>
 * @param count Number of elements to convert (default: full array)
 * @return std::vector<T> containing current values
 *
 * @code
 * std::array<Signal<bool>, 16> muteStates;
 * // ...
 * std::vector<bool> snapshot = toVector(muteStates, activeCount);
 * @endcode
 */
template <typename T, size_t N, size_t MaxSubs = 4>
[[nodiscard]] std::vector<T> toVector(const std::array<Signal<T, MaxSubs>, N>& signals,
                                       size_t count = N) {
    std::vector<T> result;
    size_t n = (count < N) ? count : N;
    result.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        result.push_back(signals[i].get());
    }
    return result;
}

}  // namespace oc::state
