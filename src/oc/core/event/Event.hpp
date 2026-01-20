#pragma once

#include <oc/types/Event.hpp>
#include "EventTypes.hpp"

namespace oc::core::event {

// Re-export base Event from oc:: namespace for backwards compatibility
using oc::Event;
using oc::EventType;
using oc::EventCategoryType;

}  // namespace oc::core::event
