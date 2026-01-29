#pragma once

#include <oc/type/Event.hpp>

namespace oc::core::event {

// Re-export event categories from oc::type::EventCategory
namespace EventCategory {
inline constexpr oc::type::EventCategoryType SYSTEM = oc::type::EventCategory::SYSTEM;
inline constexpr oc::type::EventCategoryType USER_INPUT = oc::type::EventCategory::USER_INPUT;
inline constexpr oc::type::EventCategoryType MIDI = oc::type::EventCategory::MIDI;
inline constexpr oc::type::EventCategoryType UI = oc::type::EventCategory::UI;
inline constexpr oc::type::EventCategoryType CONTEXT = oc::type::EventCategory::CONTEXT;
}  // namespace EventCategory

/// System-level event types
namespace SystemEvent {
constexpr oc::type::EventType MODE_CHANGE = 4001;
// Avoid using the identifier ERROR because it commonly collides with Windows headers.
constexpr oc::type::EventType ERROR_EVENT = 4002;
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
