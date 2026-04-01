#include "EventBus.hpp"

#include <config/PlatformCompat.hpp>
#include <oc/log/Log.hpp>

namespace oc::core::event {

FLASHMEM EventBus::EventBus() : next_id_(1) {}

FLASHMEM interface::SubscriptionID EventBus::on(oc::type::EventCategoryType category,
                                                oc::type::EventType type,
                                                interface::EventCallback callback) {
    if (!callback) return 0;

    const uint32_t key = makeKey(category, type);
    TopicSlot* topic = findOrCreateTopic_(key);
    if (topic == nullptr) {
        if constexpr (ENABLE_STATS) {
            stats_.overflowCount++;
        }
        OC_LOG_WARN(
            "EventBus: topic capacity reached category={} type={} topics={} max_topics={}",
            static_cast<unsigned>(category),
            static_cast<unsigned>(type),
            topic_count_,
            MAX_EVENT_TOPICS);
        return 0;
    }

    const size_t activeCount = activeSubscriberCountForKey_(key);
    if (activeCount >= MAX_SUBSCRIBERS_PER_EVENT) {
        if constexpr (ENABLE_STATS) {
            stats_.overflowCount++;
        }
        OC_LOG_WARN(
            "EventBus: subscriber capacity reached category={} type={} active={} max={}",
            static_cast<unsigned>(category),
            static_cast<unsigned>(type),
            activeCount,
            MAX_SUBSCRIBERS_PER_EVENT);
        return 0;
    }

    SubscriptionSlot* target = nullptr;
    for (auto& slot : subscriptions_) {
        if (slot.id == 0 || !slot.alive) {
            target = &slot;
            break;
        }
    }

    if (target == nullptr) {
        if constexpr (ENABLE_STATS) {
            stats_.overflowCount++;
        }
        OC_LOG_WARN(
            "EventBus: global subscription capacity reached topic_key={} total_max={}",
            key,
            MAX_EVENT_SUBSCRIPTIONS);
        return 0;
    }

    interface::SubscriptionID id = next_id_++;
    if (id == 0) {
        id = next_id_++;
    }

    target->id = id;
    target->key = key;
    target->callback = std::move(callback);
    target->alive = true;

    if constexpr (ENABLE_STATS) {
        stats_.totalSubscribed++;
        const size_t total = getSubscriberCount();
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

    const uint32_t key = makeKey(event.getCategory(), event.getType());
    if (findTopic_(key) == nullptr) {
        return;
    }

    ++emit_depth_;
    for (auto& slot : subscriptions_) {
        if (slot.alive && slot.key == key && slot.callback) {
            slot.callback(event);
        }
    }
    --emit_depth_;

    if (emit_depth_ == 0) {
        autoCompactIfNeeded();
    }
}

FLASHMEM void EventBus::off(interface::SubscriptionID id) {
    if (id == 0) {
        return;
    }

    for (auto& slot : subscriptions_) {
        if (slot.id != id) {
            continue;
        }

        if (!slot.alive) {
            return;
        }

        slot.alive = false;
        slot.callback = nullptr;
        ++dead_count_;

        if constexpr (ENABLE_STATS) {
            stats_.totalUnsubscribed++;
        }

        if (emit_depth_ == 0) {
            autoCompactIfNeeded();
        }
        return;
    }
}

FLASHMEM void EventBus::clear() {
    for (auto& topic : topics_) {
        clearTopic_(topic);
    }
    for (auto& slot : subscriptions_) {
        slot.id = 0;
        slot.key = 0;
        slot.callback = nullptr;
        slot.alive = false;
    }
    next_id_ = 1;
    topic_count_ = 0;
    dead_count_ = 0;
    emit_depth_ = 0;
}

FLASHMEM size_t EventBus::getSubscriberCount() const {
    size_t count = 0;
    for (const auto& slot : subscriptions_) {
        if (slot.alive) {
            ++count;
        }
    }
    return count;
}

FLASHMEM void EventBus::compact() {
    for (auto& slot : subscriptions_) {
        if (!slot.alive) {
            slot.id = 0;
            slot.key = 0;
            slot.callback = nullptr;
        }
    }
    reclaimEmptyTopics_();
    dead_count_ = 0;

    if constexpr (ENABLE_STATS) {
        stats_.totalCompactions++;
    }
}

FLASHMEM void EventBus::autoCompactIfNeeded() {
    if (emit_depth_ == 0 && dead_count_ >= EVENTBUS_COMPACT_THRESHOLD) {
        compact();
    }
}

uint32_t EventBus::makeKey(oc::type::EventCategoryType category, oc::type::EventType type) const {
    return (static_cast<uint32_t>(category) << 16) | type;
}

EventBus::TopicSlot* EventBus::findTopic_(uint32_t key) {
    for (auto& topic : topics_) {
        if (topic.used && topic.key == key) {
            return &topic;
        }
    }
    return nullptr;
}

const EventBus::TopicSlot* EventBus::findTopic_(uint32_t key) const {
    for (const auto& topic : topics_) {
        if (topic.used && topic.key == key) {
            return &topic;
        }
    }
    return nullptr;
}

EventBus::TopicSlot* EventBus::findOrCreateTopic_(uint32_t key) {
    if (auto* topic = findTopic_(key)) {
        return topic;
    }

    for (auto& topic : topics_) {
        if (!topic.used) {
            topic.used = true;
            topic.key = key;
            ++topic_count_;
            return &topic;
        }
    }

    if (dead_count_ != 0 && emit_depth_ == 0) {
        compact();
        for (auto& topic : topics_) {
            if (!topic.used) {
                topic.used = true;
                topic.key = key;
                ++topic_count_;
                return &topic;
            }
        }
    }

    return nullptr;
}

bool EventBus::topicHasActiveSubscribers_(uint32_t key) const {
    for (const auto& slot : subscriptions_) {
        if (slot.alive && slot.key == key) {
            return true;
        }
    }
    return false;
}

size_t EventBus::activeSubscriberCountForKey_(uint32_t key) const {
    size_t count = 0;
    for (const auto& slot : subscriptions_) {
        if (slot.alive && slot.key == key) {
            ++count;
        }
    }
    return count;
}

void EventBus::clearTopic_(TopicSlot& topic) {
    topic.key = 0;
    topic.used = false;
}

void EventBus::reclaimEmptyTopics_() {
    size_t usedCount = 0;
    for (auto& topic : topics_) {
        if (!topic.used) {
            continue;
        }

        if (!topicHasActiveSubscribers_(topic.key)) {
            clearTopic_(topic);
            continue;
        }

        ++usedCount;
    }
    topic_count_ = usedCount;
}

}  // namespace oc::core::event
