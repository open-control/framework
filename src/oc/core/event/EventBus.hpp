#pragma once

#include <cstddef>
#include <unordered_map>
#include <vector>

#include "IEventBus.hpp"

#include <oc/Config.hpp>

namespace oc::core::event {

using oc::config::MAX_SUBSCRIBERS_PER_EVENT;
using oc::config::EVENTBUS_COMPACT_THRESHOLD;
using oc::config::ENABLE_STATS;

/**
 * @brief Default implementation of IEventBus using pub/sub pattern
 *
 * Uses category+type key for O(1) lookup of subscribers.
 * Supports configurable limits via OC_MAX_SUBSCRIBERS_PER_EVENT.
 */
class EventBus : public IEventBus {
public:
    EventBus();

    SubscriptionID on(EventCategoryType category, EventType type, EventCallback callback) override;
    void emit(const Event& event) override;
    void off(SubscriptionID id) override;

    /// Remove all subscriptions and reset ID counter
    void clear();

    /// Get total number of active subscriptions (excludes dead entries pending compaction)
    size_t getSubscriberCount() const;

    /// Remove dead subscriptions from internal storage
    void compact();

    /// Get number of dead entries pending compaction
    size_t deadCount() const { return dead_count_; }

    /// Get maximum subscribers per event type
    static constexpr size_t maxSubscribersPerEvent() { return MAX_SUBSCRIBERS_PER_EVENT; }

    // ═══════════════════════════════════════════════════════════════════════════
    // Statistics (only available when OC_ENABLE_STATS=1)
    // ═══════════════════════════════════════════════════════════════════════════

    struct Stats {
        size_t peakSubscribers = 0;   ///< Maximum total subscribers ever seen
        size_t totalSubscribed = 0;   ///< Total on() calls
        size_t totalUnsubscribed = 0; ///< Total off() calls
        size_t totalEmitted = 0;      ///< Total emit() calls
        size_t totalCompactions = 0;  ///< Times compact() was called
        size_t overflowCount = 0;     ///< Subscriptions rejected due to limit
    };

    [[nodiscard]] const Stats& stats() const { return stats_; }
    void resetStats() { stats_ = {}; }

private:
    struct Subscription {
        SubscriptionID id;
        EventCallback callback;
        bool alive = true;
    };

    uint32_t makeKey(EventCategoryType category, EventType type) const;
    void autoCompactIfNeeded();

    std::unordered_map<uint32_t, std::vector<Subscription>> subscriptions_;
    SubscriptionID next_id_;
    size_t dead_count_ = 0;  ///< Number of dead entries across all vectors
    Stats stats_{};
};

}  // namespace oc::core::event
