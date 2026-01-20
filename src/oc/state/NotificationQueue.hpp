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
#include <unordered_set>
#include <utility>
#include <vector>

#include <oc/Config.hpp>

namespace oc::state {

using oc::MAX_PENDING_NOTIFICATIONS;
using oc::ENABLE_STATS;

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

private:
    /// Hash function for Key (signal_ptr, slot_index)
    struct KeyHash {
        size_t operator()(const Key& k) const noexcept {
            // Combine pointer and slot index into hash
            auto h1 = std::hash<void*>{}(k.first);
            auto h2 = std::hash<size_t>{}(k.second);
            return h1 ^ (h2 << 1);
        }
    };

public:

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

    // ═══════════════════════════════════════════════════════════════════════════
    // Batch Operations
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief RAII guard for batch updates
     *
     * Automatically defers notifications during scope and flushes on exit.
     * Useful for updating multiple signals atomically.
     *
     * @code
     * {
     *     auto batch = NotificationQueue::instance().batch();
     *     state.param1.set(1);
     *     state.param2.set(2);
     *     state.param3.set(3);
     * }  // Flush happens here - all callbacks called with final values
     * @endcode
     */
    class BatchGuard {
    public:
        explicit BatchGuard(NotificationQueue& queue) : queue_(queue), wasDeferred_(queue.deferredMode_) {
            queue_.deferredMode_ = true;
        }
        ~BatchGuard() {
            queue_.flush();
            queue_.deferredMode_ = wasDeferred_;
        }
        BatchGuard(const BatchGuard&) = delete;
        BatchGuard& operator=(const BatchGuard&) = delete;
    private:
        NotificationQueue& queue_;
        bool wasDeferred_;
    };

    /**
     * @brief Create a batch guard for scoped deferred updates
     * @return RAII guard that flushes on destruction
     */
    [[nodiscard]] BatchGuard batch() { return BatchGuard(*this); }

    /**
     * @brief Get maximum allowed pending notifications
     */
    [[nodiscard]] static constexpr size_t maxPending() { return MAX_PENDING_NOTIFICATIONS; }

    /**
     * @brief Check if queue reached capacity (overflow occurred)
     */
    [[nodiscard]] bool hasOverflowed() const { return overflowCount_ > 0; }

    /**
     * @brief Get number of dropped notifications due to overflow
     */
    [[nodiscard]] size_t overflowCount() const { return overflowCount_; }

    /**
     * @brief Reset overflow counter
     */
    void resetOverflowCount() { overflowCount_ = 0; }

    // ═══════════════════════════════════════════════════════════════════════════
    // Statistics (only available when OC_ENABLE_STATS=1)
    // ═══════════════════════════════════════════════════════════════════════════

    struct Stats {
        size_t peakPending = 0;      ///< Maximum pending count ever seen
        size_t totalEnqueued = 0;    ///< Total notifications enqueued
        size_t totalCoalesced = 0;   ///< Total duplicates coalesced
        size_t totalFlushed = 0;     ///< Total notifications executed
    };

    /**
     * @brief Get statistics (only meaningful when ENABLE_STATS=true)
     */
    [[nodiscard]] const Stats& stats() const { return stats_; }

    /**
     * @brief Reset statistics
     */
    void resetStats() { stats_ = {}; }

private:
    NotificationQueue() = default;

    struct PendingNotification {
        Key key;
        NotifyFn fn;
    };

    std::vector<PendingNotification> pending_;
    std::unordered_set<Key, KeyHash> pending_keys_;  ///< O(1) coalescing lookup
    bool deferredMode_ = true;
    bool isFlushing_ = false;  ///< Prevent re-entrancy issues
    size_t overflowCount_ = 0; ///< Number of dropped notifications
    Stats stats_{};            ///< Runtime statistics (when enabled)
};

}  // namespace oc::state
