#pragma once

#include <array>
#include <cstddef>

#include <oc/interface/IEventBus.hpp>

#include <oc/Config.hpp>

namespace oc::core::event {

using oc::MAX_SUBSCRIBERS_PER_EVENT;
using oc::MAX_EVENT_TOPICS;
using oc::MAX_EVENT_SUBSCRIPTIONS;
using oc::EVENTBUS_COMPACT_THRESHOLD;

/**
 * @brief Default implementation of IEventBus using pub/sub pattern
 *
 * Uses a fixed-capacity topic table keyed by category+type, with bounded
 * subscribers per topic. This keeps memory usage deterministic while
 * preserving the familiar on/off/emit API.
 */
class EventBus : public interface::IEventBus {
public:
    EventBus();

    interface::SubscriptionID on(oc::type::EventCategoryType category, oc::type::EventType type, interface::EventCallback callback) override;
    void emit(const oc::type::Event& event) override;
    void off(interface::SubscriptionID id) override;

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

    /// Get maximum distinct event topics supported
    static constexpr size_t maxTopics() { return MAX_EVENT_TOPICS; }

    /// Get maximum total subscriptions supported across all topics
    static constexpr size_t maxSubscriptions() { return MAX_EVENT_SUBSCRIPTIONS; }

private:
    struct SubscriptionSlot {
        interface::SubscriptionID id = 0;
        uint32_t key = 0;
        interface::EventCallback callback{};
        bool alive = false;
    };

    struct TopicSlot {
        uint32_t key = 0;
        bool used = false;
    };

    uint32_t makeKey(oc::type::EventCategoryType category, oc::type::EventType type) const;
    TopicSlot* findTopic_(uint32_t key);
    const TopicSlot* findTopic_(uint32_t key) const;
    TopicSlot* findOrCreateTopic_(uint32_t key);
    bool topicHasActiveSubscribers_(uint32_t key) const;
    size_t activeSubscriberCountForKey_(uint32_t key) const;
    void clearTopic_(TopicSlot& topic);
    void reclaimEmptyTopics_();
    void autoCompactIfNeeded();

    std::array<TopicSlot, MAX_EVENT_TOPICS> topics_{};
    std::array<SubscriptionSlot, MAX_EVENT_SUBSCRIPTIONS> subscriptions_{};
    interface::SubscriptionID next_id_;
    size_t topic_count_ = 0;
    size_t dead_count_ = 0;  ///< Number of dead subscriber slots pending compaction
    size_t emit_depth_ = 0;
};

}  // namespace oc::core::event
