#pragma once

#include "EventTypes.hpp"

namespace oc::core::event {

/**
 * @brief Base class for all events in the system
 *
 * Events are categorized (SYSTEM, USER_INPUT, MIDI, etc.) and typed
 * for efficient subscription filtering.
 */
class Event {
public:
    Event(EventCategoryType category, EventType type) : category_(category), type_(type) {}
    virtual ~Event() = default;

    EventCategoryType getCategory() const { return category_; }
    EventType getType() const { return type_; }

protected:
    EventCategoryType category_;
    EventType type_;
};

}  // namespace oc::core::event
