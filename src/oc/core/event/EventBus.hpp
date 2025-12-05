#pragma once

#include <cstddef>
#include <map>
#include <vector>

#include "IEventBus.hpp"

namespace oc::core::event {

/**
 * @brief Default implementation of IEventBus using pub/sub pattern
 *
 * Uses category+type key for O(1) lookup of subscribers.
 */
class EventBus : public IEventBus {
public:
    EventBus();

    SubscriptionID on(EventCategoryType category, EventType type, EventCallback callback) override;
    void emit(const Event& event) override;
    void off(SubscriptionID id) override;

    /// Remove all subscriptions and reset ID counter
    void clear();

    /// Get total number of active subscriptions
    size_t getSubscriberCount() const;

private:
    struct Subscription {
        SubscriptionID id;
        EventCallback callback;
    };

    uint32_t makeKey(EventCategoryType category, EventType type) const;

    std::map<uint32_t, std::vector<Subscription>> subscriptions_;
    SubscriptionID next_id_;
};

}  // namespace oc::core::event
