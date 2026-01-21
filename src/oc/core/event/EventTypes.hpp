#pragma once

#include <oc/type/Event.hpp>

namespace oc::core::event {

// Re-export types from oc:: for use within this namespace

// Import event categories from oc::type::EventCategory
namespace EventCategory {
using namespace oc::type::EventCategory;
}  // namespace EventCategory

/// System-level event types
namespace SystemEvent {
constexpr oc::type::EventType MODE_CHANGE = 4001;
constexpr oc::type::EventType ERROR = 4002;
constexpr oc::type::EventType BOOT_COMPLETE = 4003;
constexpr oc::type::EventType CONTEXT_REGISTERED = 4004;
constexpr oc::type::EventType CONTEXT_ACTIVATED = 4005;
constexpr oc::type::EventType CONTEXT_DEACTIVATED = 4006;
constexpr oc::type::EventType CONTEXT_ERROR = 4007;
}  // namespace SystemEvent

/// User input event types
namespace InputEvent {
constexpr oc::type::EventType ENCODER_CHANGED = 100;
constexpr oc::type::EventType BUTTON_PRESS = 101;
constexpr oc::type::EventType BUTTON_RELEASE = 102;
constexpr oc::type::EventType BUTTON_LONG_PRESS = 5;
constexpr oc::type::EventType BUTTON_COMBO = 6;
constexpr oc::type::EventType BUTTON_DOUBLE_PRESS = 7;
}  // namespace InputEvent

/// MIDI event types
namespace MidiEvent {
constexpr oc::type::EventType CC = 2002;
constexpr oc::type::EventType NOTE_ON = 2000;
constexpr oc::type::EventType NOTE_OFF = 2001;
constexpr oc::type::EventType PROGRAM_CHANGE = 2003;
constexpr oc::type::EventType PITCH_BEND = 2004;
constexpr oc::type::EventType SYSEX = 2006;
}  // namespace MidiEvent

}  // namespace oc::core::event
