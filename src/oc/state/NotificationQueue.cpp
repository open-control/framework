/**
 * @file NotificationQueue.cpp
 * @brief Implementation of deferred notification queue
 */

#include "NotificationQueue.hpp"

#include <algorithm>

#include <oc/log/Log.hpp>

namespace oc::state {

NotificationQueue& NotificationQueue::instance() {
    static NotificationQueue instance;
    return instance;
}

void NotificationQueue::enqueue(Key key, NotifyFn fn) {
    // Immediate mode: execute directly (legacy behavior)
    if (!deferredMode_) {
        if constexpr (ENABLE_STATS) {
            stats_.totalEnqueued++;
            stats_.totalFlushed++;
        }
        fn();
        return;
    }

    // O(1) check if this key is already queued (coalescing)
    if (pending_keys_.count(key)) {
        // Already queued - ignore duplicate
        // The existing entry will use the final value at flush time
        if constexpr (ENABLE_STATS) {
            stats_.totalCoalesced++;
        }
        return;
    }

    // Check overflow before adding
    if (pending_.size() >= MAX_PENDING_NOTIFICATIONS) {
        overflowCount_++;
        OC_LOG_WARN("NotificationQueue overflow! Dropped notification (total dropped: {})",
                    overflowCount_);
        return;
    }

    // Add to queue and tracking set
    pending_keys_.insert(key);
    pending_.push_back({key, std::move(fn)});

    if constexpr (ENABLE_STATS) {
        stats_.totalEnqueued++;
        if (pending_.size() > stats_.peakPending) {
            stats_.peakPending = pending_.size();
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
    while (!pending_.empty()) {
        // Move to local to handle notifications that enqueue more notifications
        auto toProcess = std::move(pending_);
        pending_.clear();
        pending_keys_.clear();

        // Execute all pending notifications
        for (auto& p : toProcess) {
            if constexpr (ENABLE_STATS) {
                stats_.totalFlushed++;
            }
            p.fn();
        }
    }

    isFlushing_ = false;
}

}  // namespace oc::state
