/**
 * @file NotificationQueue.cpp
 * @brief Implementation of deferred notification queue
 */

#include "NotificationQueue.hpp"

#include <oc/diagnostics/Performance.hpp>
#include <oc/log/Log.hpp>

namespace oc::state {

#if OC_ENABLE_STATS
namespace {

constexpr size_t POINTER_HEX_DIGITS = sizeof(uintptr_t) * 2U;
constexpr size_t POINTER_HEX_BUFFER_SIZE = 2U + POINTER_HEX_DIGITS + 1U;

void formatPointerHex(const void* value, char (&buffer)[POINTER_HEX_BUFFER_SIZE]) {
    constexpr char HEX_DIGITS[] = "0123456789abcdef";
    const uintptr_t address = reinterpret_cast<uintptr_t>(value);

    buffer[0] = '0';
    buffer[1] = 'x';
    for (size_t digitIndex = 0; digitIndex < POINTER_HEX_DIGITS; ++digitIndex) {
        const size_t remainingDigits = POINTER_HEX_DIGITS - digitIndex - 1U;
        const auto digit = static_cast<uint8_t>(
            (address >> (remainingDigits * 4U)) & static_cast<uintptr_t>(0x0FU)
        );
        buffer[2U + digitIndex] = HEX_DIGITS[digit];
    }
    buffer[POINTER_HEX_BUFFER_SIZE - 1U] = '\0';
}

}  // namespace
#endif

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

void NotificationQueue::enqueue(Key key, void* context, NotifyFn fn
#if OC_ENABLE_STATS
                                , const char* debugLabel
#endif
) {
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
#if OC_ENABLE_STATS
        const bool hasCurrent = isFlushing_ && currentProcessingKey_ != nullptr;
        const Key currentKey = hasCurrent
            ? *currentProcessingKey_
            : Key{nullptr, 0};
        char currentOwnerHex[POINTER_HEX_BUFFER_SIZE];
        char rejectedOwnerHex[POINTER_HEX_BUFFER_SIZE];
        formatPointerHex(currentKey.first, currentOwnerHex);
        formatPointerHex(key.first, rejectedOwnerHex);
        OC_LOG_WARN(
            "[NotificationQueue] overflow dropped={} pending={} capacity={} highWater={} "
            "wave={} currentLabel={} currentOwner={} currentSlot={} rejectedLabel={} "
            "rejectedOwner={} rejectedSlot={}",
            overflowCount_,
            pendingCount_,
            MAX_PENDING_NOTIFICATIONS,
            flushHighWater_,
            currentWave_,
            hasCurrent
                ? (currentDebugLabel_ ? currentDebugLabel_ : "<unnamed>")
                : "<none>",
            currentOwnerHex,
            currentKey.second,
            debugLabel ? debugLabel : "<unnamed>",
            rejectedOwnerHex,
            key.second
        );
#else
        OC_LOG_WARN("NotificationQueue overflow! Dropped notification (total dropped: {})",
                    overflowCount_);
#endif
        return;
    }

    // Add to fixed-capacity queue
    pending_[pendingCount_] = Entry{key, context, fn};
    ++pendingCount_;

#if OC_ENABLE_STATS
    if (pendingCount_ > flushHighWater_) {
        flushHighWater_ = pendingCount_;
    }
#endif
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
#if OC_ENABLE_STATS
    currentWave_ = 0;
    currentProcessingKey_ = nullptr;
    currentDebugLabel_ = nullptr;
#endif

    // Process until queue is empty
    // (new notifications during processing go to next iteration)
    while (pendingCount_ != 0) {
#if OC_ENABLE_STATS
        ++currentWave_;
#endif
        processingCount_ = pendingCount_;
        for (size_t i = 0; i < processingCount_; ++i) {
            processing_[i] = pending_[i];
        }
        pendingCount_ = 0;

        // Execute all pending notifications
        for (size_t i = 0; i < processingCount_; ++i) {
            auto& entry = processing_[i];
            if (entry.fn != nullptr) {
#if OC_ENABLE_STATS
                const Key currentKey = entry.key;
                currentProcessingKey_ = &currentKey;
#endif
                entry.fn(entry.context, entry.key.second);
#if OC_ENABLE_STATS
                currentProcessingKey_ = nullptr;
                currentDebugLabel_ = nullptr;
#endif
            }
        }
        for (size_t i = 0; i < processingCount_; ++i) {
            processing_[i] = {};
        }
        processingCount_ = 0;
    }

#if OC_ENABLE_STATS
    // PerformanceReporter exposes these as unitAAvg (peak pending per flush)
    // and unitBAvg (wave count per flush).
    OC_PERF_UNITS(
        perfFlush,
        static_cast<uint32_t>(flushHighWater_),
        static_cast<uint32_t>(currentWave_)
    );
    flushHighWater_ = 0;
    currentWave_ = 0;
    currentProcessingKey_ = nullptr;
    currentDebugLabel_ = nullptr;
#endif
    isFlushing_ = false;
}

}  // namespace oc::state
