/**
 * @file NotificationQueue.cpp
 * @brief Implementation of deferred notification queue
 */

#include "NotificationQueue.hpp"

#include <oc/diagnostics/Performance.hpp>
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
        fn(context, key.second);
        return;
    }

    // O(N) check against fixed-capacity queue (deterministic, allocation-free)
    if (containsKey_(pending_, pendingCount_, key)) {
        // Already queued - ignore duplicate
        // The existing entry will use the final value at flush time
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

}

void NotificationQueue::cancel(Key key) {
    cancelMatching_(key.first, key.second, true);
}

void NotificationQueue::cancelOwner(void* owner) {
    if (owner == nullptr) return;
    cancelMatching_(owner, 0, false);
}

void NotificationQueue::cancelMatching_(void* owner, size_t slot, bool matchSlot) {
    size_t write = 0;
    for (size_t read = 0; read < pendingCount_; ++read) {
        const auto& entry = pending_[read];
        const bool matches = entry.key.first == owner &&
                             (!matchSlot || entry.key.second == slot);
        if (!matches) {
            if (write != read) pending_[write] = entry;
            ++write;
        }
    }
    for (size_t i = write; i < pendingCount_; ++i) {
        pending_[i] = {};
    }
    pendingCount_ = write;

    for (size_t i = 0; i < processingCount_; ++i) {
        auto& entry = processing_[i];
        if (entry.key.first == owner && (!matchSlot || entry.key.second == slot)) {
            entry = {};
        }
    }
}

void NotificationQueue::flush() {
    if (isFlushing_) {
        // The active outer flush drains notifications enqueued by this callback
        // in its next wave.
        return;
    }

    isFlushing_ = true;
    OC_PERF_SCOPE(perfFlush, "notifications.flush");

    // Process until queue is empty
    // (new notifications during processing go to next iteration)
    while (pendingCount_ != 0) {
        processingCount_ = pendingCount_;
        for (size_t i = 0; i < processingCount_; ++i) {
            processing_[i] = pending_[i];
        }
        pendingCount_ = 0;

        // Execute all pending notifications
        for (size_t i = 0; i < processingCount_; ++i) {
            auto& entry = processing_[i];
            if (entry.fn != nullptr) {
                entry.fn(entry.context, entry.key.second);
            }
        }
        for (size_t i = 0; i < processingCount_; ++i) {
            processing_[i] = {};
        }
        processingCount_ = 0;
    }

    isFlushing_ = false;
}

}  // namespace oc::state
