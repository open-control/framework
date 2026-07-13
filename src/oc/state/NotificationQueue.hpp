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
 * 6. Notifications created by a callback run in a following wave before the
 *    outer flush returns
 *
 * ## Benefits
 *
 * - **Transparent**: No changes required in handlers or views
 * - **Automatic**: Deduplication happens by design
 * - **Efficient**: Duplicate pending callbacks are coalesced within each wave
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

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

#include <oc/Config.hpp>

namespace oc::state {

using oc::MAX_PENDING_NOTIFICATIONS;

/**
 * @brief Singleton queue for deferred signal notifications
 *
 * Thread safety: NOT thread-safe (single-threaded embedded use only)
 */
class alignas(std::max_align_t) NotificationQueue {
public:
    /// Notification key: (signal_ptr, slot_index) uniquely identifies a callback
    using Key = std::pair<void*, size_t>;

    /// Notification function (context + slot for deferred execution)
    using NotifyFn = void (*)(void*, size_t);

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
     * @param context Opaque callback context
     * @param fn Function to call at flush time
     */
    void enqueue(Key key, void* context, NotifyFn fn
#if OC_ENABLE_STATS
                 , const char* debugLabel = nullptr
#endif
    );

#if OC_ENABLE_STATS
    /**
     * @brief Temporarily identify the notification callback currently running
     *
     * Diagnostic builds use this scope to correlate an overflow with the
     * semantic callback that caused it. The label is transient and is not
     * stored in queue entries.
     */
    class CurrentLabelScope {
    public:
        CurrentLabelScope(NotificationQueue& queue, const char* label)
            : queue_(queue), previous_(queue.currentDebugLabel_) {
            queue_.currentDebugLabel_ = label;
        }

        ~CurrentLabelScope() {
            queue_.currentDebugLabel_ = previous_;
        }

        CurrentLabelScope(const CurrentLabelScope&) = delete;
        CurrentLabelScope& operator=(const CurrentLabelScope&) = delete;

    private:
        NotificationQueue& queue_;
        const char* previous_ = nullptr;
    };

    [[nodiscard]] CurrentLabelScope scopedCurrentLabel(const char* label) {
        return CurrentLabelScope(*this, label);
    }
#endif

    /** Remove a deferred callback before its owner or context is destroyed. */
    void cancel(Key key);

    /** Remove every deferred callback whose key is owned by `owner`. */
    void cancelOwner(void* owner);

    /**
     * @brief Execute all pending notifications and clear the queue
     *
     * Notifications triggered during flush are processed in a following wave
     * of the same outer flush. A nested flush call returns immediately; the
     * outer call remains responsible for draining the reactive chain.
     */
    void flush();

    /**
     * @brief Check if there are pending notifications
     */
    [[nodiscard]] bool hasPending() const { return pendingCount_ != 0; }

    /**
     * @brief Get count of pending notifications (for debugging)
     */
    [[nodiscard]] size_t pendingCount() const { return pendingCount_; }

    /**
     * @brief Enable/disable deferred mode
     *
     * When disabled, notifications execute immediately.
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

private:
    NotificationQueue() = default;

    struct Entry {
        Key key{nullptr, 0};
        void* context = nullptr;
        NotifyFn fn = nullptr;
    };

    bool containsKey_(const std::array<Entry, MAX_PENDING_NOTIFICATIONS>& entries,
                      size_t count,
                      Key key) const;
    void cancelMatching_(void* owner, size_t slot, bool matchSlot);

    std::array<Entry, MAX_PENDING_NOTIFICATIONS> pending_{};
    std::array<Entry, MAX_PENDING_NOTIFICATIONS> processing_{};
    size_t pendingCount_ = 0;
    size_t processingCount_ = 0;
    bool deferredMode_ = true;
    bool isFlushing_ = false;  ///< Prevent re-entrancy issues
    size_t overflowCount_ = 0; ///< Number of dropped notifications
#if OC_ENABLE_STATS
    size_t flushHighWater_ = 0;
    size_t currentWave_ = 0;
    const Key* currentProcessingKey_ = nullptr;
    const char* currentDebugLabel_ = nullptr;
#endif
};

}  // namespace oc::state
