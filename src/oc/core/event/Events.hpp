#pragma once

#include <cstdint>

#include <oc/types/Event.hpp>
#include "EventTypes.hpp"

#include <oc/types/Ids.hpp>
#include <oc/types/Callbacks.hpp>

namespace oc::core::event {

/// Encoder rotation event
class EncoderChangedEvent : public Event {
public:
    EncoderChangedEvent(EncoderID id, float value)
        : Event(EventCategory::USER_INPUT, InputEvent::ENCODER_CHANGED),
          encoderId(id),
          normalizedValue(value) {}

    EncoderID encoderId;
    float normalizedValue;
};

/// Button press event
class ButtonPressEvent : public Event {
public:
    ButtonPressEvent(ButtonID id, bool isPressed)
        : Event(EventCategory::USER_INPUT, InputEvent::BUTTON_PRESS),
          buttonId(id),
          pressed(isPressed) {}

    ButtonID buttonId;
    bool pressed;
};

/// Button release event
class ButtonReleaseEvent : public Event {
public:
    explicit ButtonReleaseEvent(ButtonID id)
        : Event(EventCategory::USER_INPUT, InputEvent::BUTTON_RELEASE), buttonId(id) {}

    ButtonID buttonId;
};

/// MIDI Control Change event
class MidiCCEvent : public Event {
public:
    MidiCCEvent(uint8_t ch, uint8_t cc, uint8_t val)
        : Event(EventCategory::MIDI, MidiEvent::CC),
          channel(ch),
          controller(cc),
          value(val) {}

    uint8_t channel;
    uint8_t controller;
    uint8_t value;
};

/// MIDI Note On event
class MidiNoteOnEvent : public Event {
public:
    MidiNoteOnEvent(uint8_t ch, uint8_t n, uint8_t vel)
        : Event(EventCategory::MIDI, MidiEvent::NOTE_ON),
          channel(ch),
          note(n),
          velocity(vel) {}

    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
};

/// MIDI Note Off event
class MidiNoteOffEvent : public Event {
public:
    MidiNoteOffEvent(uint8_t ch, uint8_t n, uint8_t vel)
        : Event(EventCategory::MIDI, MidiEvent::NOTE_OFF),
          channel(ch),
          note(n),
          velocity(vel) {}

    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
};

/// MIDI System Exclusive event
class SysExEvent : public Event {
public:
    SysExEvent(const uint8_t* d, uint16_t len)
        : Event(EventCategory::MIDI, MidiEvent::SYSEX), data(d), length(len) {}

    const uint8_t* data;
    uint16_t length;
};

/// System boot complete event
class SystemBootCompleteEvent : public Event {
public:
    SystemBootCompleteEvent() : Event(EventCategory::SYSTEM, SystemEvent::BOOT_COMPLETE) {}
};

// ═══════════════════════════════════════════════════════════════════
// Context Events
// ═══════════════════════════════════════════════════════════════════

/// Context was activated (switched to)
class ContextActivatedEvent : public Event {
public:
    ContextActivatedEvent(uint8_t id, const char* name)
        : Event(EventCategory::CONTEXT, SystemEvent::CONTEXT_ACTIVATED),
          contextId(id),
          contextName(name) {}

    uint8_t contextId;       ///< Numeric ID for logic
    const char* contextName; ///< Human-readable name for logging
};

/// Context was deactivated (switched away from)
class ContextDeactivatedEvent : public Event {
public:
    ContextDeactivatedEvent(uint8_t id, const char* name)
        : Event(EventCategory::CONTEXT, SystemEvent::CONTEXT_DEACTIVATED),
          contextId(id),
          contextName(name) {}

    uint8_t contextId;
    const char* contextName;
};

/// Context initialization failed
class ContextErrorEvent : public Event {
public:
    explicit ContextErrorEvent(uint8_t id)
        : Event(EventCategory::CONTEXT, SystemEvent::CONTEXT_ERROR), contextId(id) {}

    uint8_t contextId;
};

}  // namespace oc::core::event
