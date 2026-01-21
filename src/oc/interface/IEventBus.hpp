#pragma once

#include <cstdint>
#include <functional>

#include <oc/type/Event.hpp>

namespace oc::interface {

using SubscriptionID = uint16_t;
using EventCallback = std::function<void(const oc::type::Event&)>;

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
    virtual SubscriptionID on(oc::type::EventCategoryType category, oc::type::EventType type, EventCallback callback) = 0;

    /**
     * @brief Emit an event to all subscribers
     */
    virtual void emit(const oc::type::Event& event) = 0;

    /**
     * @brief Unsubscribe using the subscription ID
     */
    virtual void off(SubscriptionID id) = 0;
};

}  // namespace oc::interface
