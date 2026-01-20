#pragma once

#include <oc/interface/IEventBus.hpp>
#include <oc/core/event/Events.hpp>
#include <vector>
#include <functional>

namespace oc::test {

/**
 * @brief Mock EventBus for unit testing
 *
 * Records all subscriptions and emitted events.
 * Can replay events to subscribers.
 */
class MockEventBus : public interface::IEventBus {
public:
    using EventCallback = interface::EventCallback;
    using SubscriptionID = interface::SubscriptionID;
    using EventCategoryType = core::event::EventCategoryType;
    using EventType = core::event::EventType;
    using Event = core::event::Event;

    struct Subscription {
        SubscriptionID id;
        EventCategoryType category;
        EventType type;
        EventCallback callback;
        bool active = true;
    };

    SubscriptionID on(EventCategoryType category, EventType type, EventCallback callback) override {
        SubscriptionID id = next_id_++;
        subscriptions_.push_back({id, category, type, callback, true});
        return id;
    }

    void emit(const Event& event) override {
        emitted_count_++;
        for (auto& sub : subscriptions_) {
            if (sub.active &&
                sub.category == event.getCategory() &&
                sub.type == event.getType()) {
                sub.callback(event);
            }
        }
    }

    void off(SubscriptionID id) override {
        for (auto& sub : subscriptions_) {
            if (sub.id == id) {
                sub.active = false;
                break;
            }
        }
    }

    // Test helpers
    size_t subscriptionCount() const { return subscriptions_.size(); }
    size_t activeSubscriptionCount() const {
        size_t count = 0;
        for (const auto& sub : subscriptions_) {
            if (sub.active) count++;
        }
        return count;
    }
    size_t emittedCount() const { return emitted_count_; }
    void reset() {
        subscriptions_.clear();
        emitted_count_ = 0;
        next_id_ = 1;
    }

private:
    std::vector<Subscription> subscriptions_;
    SubscriptionID next_id_ = 1;
    size_t emitted_count_ = 0;
};

}  // namespace oc::test
