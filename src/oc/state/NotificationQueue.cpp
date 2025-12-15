/**
 * @file NotificationQueue.cpp
 * @brief Implementation of deferred notification queue
 */

#include "NotificationQueue.hpp"

#include <algorithm>

namespace oc::state {

NotificationQueue& NotificationQueue::instance() {
    static NotificationQueue instance;
    return instance;
}

void NotificationQueue::enqueue(Key key, NotifyFn fn) {
    // Immediate mode: execute directly (legacy behavior)
    if (!deferredMode_) {
        fn();
        return;
    }

    // Check if this key is already queued (coalescing)
    for (const auto& pending : pending_) {
        if (pending.key == key) {
            // Already queued - ignore duplicate
            // The existing entry will use the final value at flush time
            return;
        }
    }

    // Add to queue
    pending_.push_back({key, std::move(fn)});
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

        // Execute all pending notifications
        for (auto& p : toProcess) {
            p.fn();
        }
    }

    isFlushing_ = false;
}

}  // namespace oc::state
