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

const SubscriptionDebugContext*& currentSubscriptionDebugContext();

void reportSignalSubscriberOverflow(const char* signalLabel,
                                    size_t subscriberCount,
                                    size_t maxSubscribers,
                                    const void* signalAddress);

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

    ~Signal() {
        NotificationQueue::instance().cancelOwner(static_cast<void*>(this));
    }

    /**
     * @brief Attach an optional debug label used in overflow diagnostics
     *
     * This is intended for high-fan-out UI signals where subscriber overflow
     * is otherwise hard to localize from a crash log alone.
     */
    void setDebugLabel(const char* label) {
#if OC_ENABLE_STATS
        debug_label_ = label;
#else
        (void)label;
#endif
    }

    /**
     * @brief Return the debug label, or nullptr if unset
     */
    [[nodiscard]] const char* debugLabel() const {
#if OC_ENABLE_STATS
        return debug_label_;
#else
        return nullptr;
#endif
    }

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

                NotificationQueue::instance().enqueue(
                    key,
                    static_cast<void*>(this),
                    [](void* context, size_t slot) {
                        auto* self = static_cast<Signal*>(context);
                        if (self->callbacks_[slot]) {
                            self->callbacks_[slot](self->value_);
                        }
                    }
#if OC_ENABLE_STATS
                    , debugLabel()
#endif
                );
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
    [[nodiscard]] Subscription subscribe(Callback callback) const;

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
    // Observer registration is logically const: it does not change the
    // published value, only this bounded callback registry.
    mutable std::array<Callback, MaxSubscribers> callbacks_{};
#if OC_ENABLE_STATS
    const char* debug_label_ = nullptr;
#endif

    /// Add subscriber, returns slot index or -1 if full
    int addSubscriber(Callback callback) const {
        for (size_t i = 0; i < MaxSubscribers; ++i) {
            if (!callbacks_[i]) {
                callbacks_[i] = std::move(callback);
                return static_cast<int>(i);
            }
        }
        return -1;  // Full
    }

    /// Remove subscriber by slot index
    void removeSubscriber(int slot) const {
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

    /**
     * Cancel this subscriber's deferred callback without unsubscribing it.
     *
     * This is safe both for pending entries and for entries not yet executed
     * in the active NotificationQueue processing wave.
     */
    void cancelPendingNotification() const {
        if (owner_ != nullptr && slot_ >= 0) {
            NotificationQueue::instance().cancel(
                NotificationQueue::Key{
                    owner_,
                    static_cast<size_t>(slot_),
                }
            );
        }
    }

    /// Manually unsubscribe (also called by destructor)
    void reset() {
        if (unsubscribe_ && owner_ && slot_ >= 0) {
            // A deferred entry is keyed by the Signal address and slot. Remove
            // it before releasing the slot so it cannot target a replacement
            // subscriber that reuses the same slot before the next flush.
            cancelPendingNotification();
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
Subscription Signal<T, MaxSubscribers>::subscribe(Callback callback) const {
    if (!callback) {
        return Subscription{};
    }

    int slot = addSubscriber(std::move(callback));
    if (slot < 0) {
        // Max subscribers reached - fail loudly in debug builds
        detail::reportSignalSubscriberOverflow(
            debugLabel(),
            subscriberCount(),
            MaxSubscribers,
            static_cast<const void*>(this)
        );
        assert(false && "Signal: MaxSubscribers exceeded. Check logs for label/subscriber count.");
        return Subscription{};
    }

    // Capture 'this' to call removeSubscriber when Subscription is destroyed
    return Subscription{
        static_cast<void*>(const_cast<Signal*>(this)),
        [](void* owner, int s) {
            static_cast<Signal*>(owner)->removeSubscriber(s);
        },
        slot
    };
}

}  // namespace oc::state
