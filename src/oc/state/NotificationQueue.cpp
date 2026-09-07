/**
 * @file NotificationQueue.cpp
 * @brief Implementation of deferred notification queue
 */

#include "NotificationQueue.hpp"

#include <config/PlatformCompat.hpp>
#include <oc/diagnostics/Performance.hpp>
#include <oc/log/Log.hpp>

namespace oc::state {

#if OC_ENABLE_STATS
namespace {

constexpr size_t POINTER_HEX_DIGITS = sizeof(uintptr_t) * 2U;
constexpr size_t POINTER_HEX_BUFFER_SIZE = 2U + POINTER_HEX_DIGITS + 1U;

FLASHMEM void formatPointerHex(const void* value, char (&buffer)[POINTER_HEX_BUFFER_SIZE]) {
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
        if ((entries[i].slots & entries[i].bitFor(key)) != 0U) {
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
#if OC_ENABLE_STATS
        Entry entry{key, context, fn, debugLabel};
        invokeEntry_(entry);
#else
        fn(context, key.second);
#endif
        return;
    }

    // O(N) check against fixed-capacity queue (deterministic, allocation-free)
    if (containsKey_(pending_, pendingCount_, key)) {
        // Already queued - ignore duplicate
        // The existing entry will use the final value at flush time
        return;
    }

    // Pack only the tail, and only ascending slots. This preserves enqueue
    // order even when an owner publishes again after a different owner.
    if (pendingCount_ != 0U) {
        auto& tail = pending_[pendingCount_ - 1U];
        const uint32_t bit = tail.bitFor(key);
        if (bit > tail.slots && tail.context == context && tail.fn == fn
#if OC_ENABLE_STATS
            && tail.debugLabel == debugLabel
#endif
        ) {
            tail.slots |= bit;
            return;
        }
    }

    // Check overflow before adding
    if (pendingCount_ >= MAX_PENDING_NOTIFICATIONS) {
        overflowCount_++;
#if OC_ENABLE_STATS
        reportOverflow_(key, debugLabel);
#else
        OC_LOG_WARN("NotificationQueue overflow! Dropped notification (total dropped: {})",
                    overflowCount_);
#endif
        return;
    }

    // Add to fixed-capacity queue
    pending_[pendingCount_] = Entry{
        key,
        context,
        fn,
#if OC_ENABLE_STATS
        debugLabel,
#endif
    };
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

size_t NotificationQueue::pendingCount() const {
    size_t count = 0U;
    for (size_t i = 0; i < pendingCount_; ++i) {
        for (auto bits = pending_[i].slots; bits != 0U; bits &= bits - 1U) ++count;
    }
    return count;
}

void NotificationQueue::cancelOwner(void* owner) {
    if (owner == nullptr) return;
    cancelMatching_(owner, 0, false);
}

void NotificationQueue::cancelMatching_(void* owner, size_t slot, bool matchSlot) {
    size_t write = 0;
    for (size_t read = 0; read < pendingCount_; ++read) {
        auto& entry = pending_[read];
        if (entry.key.first == owner) {
            entry.slots &= matchSlot ? ~entry.bitFor({owner, slot}) : 0U;
        }
        if (entry.slots != 0U) {
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
        if (entry.key.first == owner) {
            entry.slots &= matchSlot ? ~entry.bitFor({owner, slot}) : 0U;
            if (entry.slots == 0U) entry = {};
        }
    }
}

#if OC_ENABLE_STATS
void NotificationQueue::invokeEntry_(Entry& entry) {
    const Key* previousKey = currentProcessingKey_;
    const char* previousLabel = currentDebugLabel_;
    const Key currentKey = entry.key;
    currentProcessingKey_ = &currentKey;
    currentDebugLabel_ = entry.debugLabel;
    {
        OC_PERF_SCOPE(
            perfCallback,
            currentDebugLabel_ ? currentDebugLabel_ : "signal.callback"
        );
        entry.fn(entry.context, entry.key.second);
    }
    currentProcessingKey_ = previousKey;
    currentDebugLabel_ = previousLabel;
}

FLASHMEM void NotificationQueue::reportOverflow_(
    Key rejectedKey,
    const char* rejectedLabel
) const {
    const bool hasCurrent = isFlushing_ && currentProcessingKey_ != nullptr;
    const Key currentKey = hasCurrent
        ? *currentProcessingKey_
        : Key{nullptr, 0};
    char currentOwnerHex[POINTER_HEX_BUFFER_SIZE];
    char rejectedOwnerHex[POINTER_HEX_BUFFER_SIZE];
    formatPointerHex(currentKey.first, currentOwnerHex);
    formatPointerHex(rejectedKey.first, rejectedOwnerHex);
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
        rejectedLabel ? rejectedLabel : "<unnamed>",
        rejectedOwnerHex,
        rejectedKey.second
    );
}
#endif

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
            while (entry.fn != nullptr && entry.slots != 0U) {
                unsigned offset = 0U;
                while ((entry.slots & (uint32_t{1} << offset)) == 0U) ++offset;
                Entry callback = entry;
                callback.key.second += offset;
                entry.slots &= ~(uint32_t{1} << offset);
                // The callback may cancel siblings or destroy its owner.
                // Keep the current invocation local; recheck the live entry.
#if OC_ENABLE_STATS
                invokeEntry_(callback);
#else
                callback.fn(callback.context, callback.key.second);
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
