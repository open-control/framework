#pragma once

#include <oc/types/Event.hpp>

namespace oc::core::event {

// Re-export types from oc:: for backwards compatibility
using oc::EventType;
using oc::EventCategoryType;

// Event categories are now in oc::EventCategory, re-export for compatibility
namespace EventCategory {
using namespace oc::EventCategory;
}  // namespace EventCategory

/// System-level event types
namespace SystemEvent {
constexpr EventType MODE_CHANGE = 4001;
constexpr EventType ERROR = 4002;
constexpr EventType BOOT_COMPLETE = 4003;
constexpr EventType CONTEXT_REGISTERED = 4004;
constexpr EventType CONTEXT_ACTIVATED = 4005;
constexpr EventType CONTEXT_DEACTIVATED = 4006;
constexpr EventType CONTEXT_ERROR = 4007;
}  // namespace SystemEvent

/// User input event types
namespace InputEvent {
constexpr EventType ENCODER_CHANGED = 100;
constexpr EventType BUTTON_PRESS = 101;
constexpr EventType BUTTON_RELEASE = 102;
constexpr EventType BUTTON_LONG_PRESS = 5;
constexpr EventType BUTTON_COMBO = 6;
constexpr EventType BUTTON_DOUBLE_PRESS = 7;
}  // namespace InputEvent

/// MIDI event types
namespace MidiEvent {
constexpr EventType CC = 2002;
constexpr EventType NOTE_ON = 2000;
constexpr EventType NOTE_OFF = 2001;
constexpr EventType PROGRAM_CHANGE = 2003;
constexpr EventType PITCH_BEND = 2004;
constexpr EventType SYSEX = 2006;
}  // namespace MidiEvent

}  // namespace oc::core::event
