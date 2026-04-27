/**
 * @file NotificationQueue.cpp
 * @brief Implementation of deferred notification queue
 */

#include "NotificationQueue.hpp"

#include <oc/log/Log.hpp>

namespace oc::state {

NotificationQueue& NotificationQueue::instance() {
    static NotificationQueue queue;
    return queue;
}

bool NotificationQueue::containsKey_(
    const std::array<Entry, MAX_PENDING_NOTIFICATIONS>& entries,
    size_t count,
    Key key) const {
    for (size_t i = 0; i < count; ++i) {
        if (entries[i].key == key) {
            return true;
        }
    }
    return false;
}

void NotificationQueue::enqueue(Key key, void* context, NotifyFn fn) {
    // Immediate mode executes synchronously.
    if (!deferredMode_) {
        if constexpr (ENABLE_STATS) {
            stats_.totalEnqueued++;
            stats_.totalFlushed++;
        }
        fn(context, key.second);
        return;
    }

    // O(N) check against fixed-capacity queue (deterministic, allocation-free)
    if (containsKey_(pending_, pendingCount_, key)) {
        // Already queued - ignore duplicate
        // The existing entry will use the final value at flush time
        if constexpr (ENABLE_STATS) {
            stats_.totalCoalesced++;
        }
        return;
    }

    // Check overflow before adding
    if (pendingCount_ >= MAX_PENDING_NOTIFICATIONS) {
        overflowCount_++;
        OC_LOG_WARN("NotificationQueue overflow! Dropped notification (total dropped: {})",
                    overflowCount_);
        return;
    }

    // Add to fixed-capacity queue
    pending_[pendingCount_] = Entry{key, context, fn};
    ++pendingCount_;

    if constexpr (ENABLE_STATS) {
        stats_.totalEnqueued++;
        if (pendingCount_ > stats_.peakPending) {
            stats_.peakPending = pendingCount_;
        }
    }
}

void NotificationQueue::flush() {
    if (isFlushing_) {
        // Re-entrancy: notifications triggered during flush
        // will be processed in the next flush() call
        return;
    }

    isFlushing_ = true;

    // Process until queue is empty
    // (new notifications during processing go to next iteration)
    while (pendingCount_ != 0) {
        const size_t processingCount = pendingCount_;
        for (size_t i = 0; i < processingCount; ++i) {
            processing_[i] = pending_[i];
        }
        pendingCount_ = 0;

        // Execute all pending notifications
        for (size_t i = 0; i < processingCount; ++i) {
            if constexpr (ENABLE_STATS) {
                stats_.totalFlushed++;
            }
            auto& entry = processing_[i];
            if (entry.fn != nullptr) {
                entry.fn(entry.context, entry.key.second);
            }
        }
    }

    isFlushing_ = false;
}

}  // namespace oc::state
