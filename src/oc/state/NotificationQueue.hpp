#pragma once

/**
 * @file NotificationQueue.hpp
 * @brief Deferred notification system with automatic coalescing
 *
 * Provides automatic batching and deduplication of Signal notifications.
 * When multiple signals change during a single tick, their subscribers
 * are called only once with the final value.
 *
 * ## How it works
 *
 * 1. Signal::set() enqueues notifications instead of executing immediately
 * 2. Each notification is identified by (signal_ptr, slot_index)
 * 3. Duplicate keys are ignored (coalescing)
 * 4. OpenControlApp::update() calls flush() at end of tick
 * 5. Callbacks execute with current (final) values
 *
 * ## Benefits
 *
 * - **Transparent**: No changes required in handlers or views
 * - **Automatic**: Deduplication happens by design
 * - **Efficient**: Each callback runs at most once per tick
 *
 * @code
 * // Handler sets 5 signals
 * state_.device.name.set(...);      // Enqueues updateDeviceInfo
 * state_.device.type.set(...);      // Already queued → ignored
 * state_.device.enabled.set(...);   // Already queued → ignored
 * state_.device.pageName.set(...);  // Already queued → ignored
 * state_.device.hasChildren.set(...); // Already queued → ignored
 *
 * // Later in app.update()
 * NotificationQueue::instance().flush();
 * // → updateDeviceInfo() called once with final values
 * @endcode
 */

#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace oc::state {

/**
 * @brief Singleton queue for deferred signal notifications
 *
 * Thread safety: NOT thread-safe (single-threaded embedded use only)
 */
class NotificationQueue {
public:
    /// Notification key: (signal_ptr, slot_index) uniquely identifies a callback
    using Key = std::pair<void*, size_t>;

    /// Notification function (captures signal/slot for deferred execution)
    using NotifyFn = std::function<void()>;

    /**
     * @brief Get the singleton instance
     */
    static NotificationQueue& instance();

    /**
     * @brief Enqueue a notification for deferred execution
     *
     * If a notification with the same key is already queued,
     * this call is ignored (coalescing).
     *
     * @param key Unique identifier (signal_ptr, slot_index)
     * @param fn Function to call at flush time
     */
    void enqueue(Key key, NotifyFn fn);

    /**
     * @brief Execute all pending notifications and clear the queue
     *
     * Notifications triggered during flush are queued for the next flush.
     * This prevents infinite loops while allowing reactive chains.
     */
    void flush();

    /**
     * @brief Check if there are pending notifications
     */
    [[nodiscard]] bool hasPending() const { return !pending_.empty(); }

    /**
     * @brief Get count of pending notifications (for debugging)
     */
    [[nodiscard]] size_t pendingCount() const { return pending_.size(); }

    /**
     * @brief Enable/disable deferred mode
     *
     * When disabled, notifications execute immediately (legacy behavior).
     * Useful for debugging or specific use cases.
     *
     * @param enabled true for deferred mode (default), false for immediate
     */
    void setDeferredMode(bool enabled) { deferredMode_ = enabled; }

    /**
     * @brief Check if deferred mode is enabled
     */
    [[nodiscard]] bool isDeferredMode() const { return deferredMode_; }

private:
    NotificationQueue() = default;

    struct PendingNotification {
        Key key;
        NotifyFn fn;
    };

    std::vector<PendingNotification> pending_;
    bool deferredMode_ = true;
    bool isFlushing_ = false;  // Prevent re-entrancy issues
};

}  // namespace oc::state
