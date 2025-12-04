#pragma once

#include <cstdint>
#include <functional>

#include "Event.hpp"

namespace oc::core::event {

using SubscriptionId = uint8_t;
using EventCallback = std::function<void(const Event&)>;

/**
 * @brief Interface for decoupled pub/sub event communication
 */
class IEventBus {
public:
    virtual ~IEventBus() = default;

    /**
     * @brief Subscribe to events of a specific category and type
     * @return Subscription ID for later unsubscription
     */
    virtual SubscriptionId on(EventCategoryType category, EventType type, EventCallback callback) = 0;

    /**
     * @brief Emit an event to all subscribers
     */
    virtual void emit(const Event& event) = 0;

    /**
     * @brief Unsubscribe using the subscription ID
     */
    virtual void off(SubscriptionId id) = 0;
};

}  // namespace oc::core::event
