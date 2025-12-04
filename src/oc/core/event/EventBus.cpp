#include "EventBus.hpp"

namespace oc::core::event {

EventBus::EventBus() : next_id_(1) {}

SubscriptionId EventBus::on(EventCategoryType category, EventType type, EventCallback callback) {
    if (!callback) return 0;

    uint32_t key = makeKey(category, type);
    SubscriptionId id = next_id_++;

    subscriptions_[key].push_back({id, callback});
    return id;
}

void EventBus::emit(const Event& event) {
    uint32_t key = makeKey(event.getCategory(), event.getType());
    auto it = subscriptions_.find(key);
    if (it != subscriptions_.end()) {
        for (const auto& sub : it->second) {
            sub.callback(event);
        }
    }
}

void EventBus::off(SubscriptionId id) {
    for (auto& pair : subscriptions_) {
        auto& list = pair.second;
        for (auto it = list.begin(); it != list.end(); ++it) {
            if (it->id == id) {
                list.erase(it);
                return;
            }
        }
    }
}

void EventBus::clear() {
    subscriptions_.clear();
    next_id_ = 1;
}

size_t EventBus::getSubscriberCount() const {
    size_t count = 0;
    for (const auto& pair : subscriptions_) {
        count += pair.second.size();
    }
    return count;
}

uint32_t EventBus::makeKey(EventCategoryType category, EventType type) const {
    return (static_cast<uint32_t>(category) << 16) | type;
}

}  // namespace oc::core::event
