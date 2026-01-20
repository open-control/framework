#pragma once

/**
 * @file Event.hpp
 * @brief Base event types (Level 0 - no internal dependencies)
 */

#include <cstdint>

namespace oc {

// ═══════════════════════════════════════════════════════════════════════════
// Event type aliases
// ═══════════════════════════════════════════════════════════════════════════

using EventType = uint16_t;
using EventCategoryType = uint8_t;

// ═══════════════════════════════════════════════════════════════════════════
// Event categories
// ═══════════════════════════════════════════════════════════════════════════

namespace EventCategory {
constexpr EventCategoryType SYSTEM = 0;
constexpr EventCategoryType USER_INPUT = 1;
constexpr EventCategoryType MIDI = 2;
constexpr EventCategoryType UI = 3;
constexpr EventCategoryType CONTEXT = 4;
}  // namespace EventCategory

// ═══════════════════════════════════════════════════════════════════════════
// Base Event class
// ═══════════════════════════════════════════════════════════════════════════

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

}  // namespace oc
