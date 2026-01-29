#include "EventBus.hpp"

#include <algorithm>

#include <oc/log/Log.hpp>

namespace oc::core::event {

EventBus::EventBus() : next_id_(1) {
    // Pre-allocate buckets to reduce dynamic allocations at runtime.
    // Typical usage: ~6 categories × ~5 event types = ~30 combinations.
    subscriptions_.reserve(32);
}

interface::SubscriptionID EventBus::on(oc::type::EventCategoryType category, oc::type::EventType type, interface::EventCallback callback) {
    if (!callback) return 0;

    uint32_t key = makeKey(category, type);
    auto& vec = subscriptions_[key];

    // Count active subscribers for this event type
    size_t activeCount = 0;
    for (const auto& sub : vec) {
        if (sub.alive) ++activeCount;
    }

    // Check limit
    if (activeCount >= MAX_SUBSCRIBERS_PER_EVENT) {
        if constexpr (ENABLE_STATS) {
            stats_.overflowCount++;
        }
        OC_LOG_WARN("EventBus: max subscribers ({}) reached for event type",
                    MAX_SUBSCRIBERS_PER_EVENT);
        return 0;
    }

    interface::SubscriptionID id = next_id_++;
    vec.push_back({id, std::move(callback), true});

    if constexpr (ENABLE_STATS) {
        stats_.totalSubscribed++;
        size_t total = getSubscriberCount();
        if (total > stats_.peakSubscribers) {
            stats_.peakSubscribers = total;
        }
    }

    return id;
}

void EventBus::emit(const oc::type::Event& event) {
    if constexpr (ENABLE_STATS) {
        stats_.totalEmitted++;
    }

    uint32_t key = makeKey(event.getCategory(), event.getType());
    auto it = subscriptions_.find(key);
    if (it != subscriptions_.end()) {
        // Iterate directly without copying - safe because off() only marks dead
        for (auto& sub : it->second) {
            if (sub.alive && sub.callback) {
                sub.callback(event);
            }
        }
    }
}

void EventBus::off(interface::SubscriptionID id) {
    // Mark as dead instead of erasing - allows safe iteration during emit()
    for (auto& pair : subscriptions_) {
        for (auto& sub : pair.second) {
            if (sub.id == id) {
                if (!sub.alive) {
                    // Idempotent: already unsubscribed
                    return;
                }
                sub.alive = false;
                sub.callback = nullptr;  // Release memory
                dead_count_++;

                if constexpr (ENABLE_STATS) {
                    stats_.totalUnsubscribed++;
                }

                // Auto-compact when threshold reached
                autoCompactIfNeeded();
                return;
            }
        }
    }
}

void EventBus::clear() {
    subscriptions_.clear();
    next_id_ = 1;
    dead_count_ = 0;
}

size_t EventBus::getSubscriberCount() const {
    size_t count = 0;
    for (const auto& pair : subscriptions_) {
        for (const auto& sub : pair.second) {
            if (sub.alive) ++count;
        }
    }
    return count;
}

void EventBus::compact() {
    for (auto& pair : subscriptions_) {
        auto& vec = pair.second;
        vec.erase(std::remove_if(vec.begin(), vec.end(),
                                  [](const Subscription& s) { return !s.alive; }),
                  vec.end());
    }
    dead_count_ = 0;

    if constexpr (ENABLE_STATS) {
        stats_.totalCompactions++;
    }
}

void EventBus::autoCompactIfNeeded() {
    if (dead_count_ >= EVENTBUS_COMPACT_THRESHOLD) {
        compact();
    }
}

uint32_t EventBus::makeKey(oc::type::EventCategoryType category, oc::type::EventType type) const {
    return (static_cast<uint32_t>(category) << 16) | type;
}

}  // namespace oc::core::event
