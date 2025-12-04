#pragma once

#include <cstdint>

#include "Event.hpp"
#include "EventTypes.hpp"

#include <oc/hal/Types.hpp>

namespace oc::core::event {

/// Encoder rotation event
class EncoderChangedEvent : public Event {
public:
    EncoderChangedEvent(hal::EncoderID id, float value)
        : Event(EventCategory::USER_INPUT, InputEvent::ENCODER_CHANGED),
          encoderId(id),
          normalizedValue(value) {}

    hal::EncoderID encoderId;
    float normalizedValue;
};

/// Button press event
class ButtonPressEvent : public Event {
public:
    ButtonPressEvent(hal::ButtonID id, bool isPressed)
        : Event(EventCategory::USER_INPUT, InputEvent::BUTTON_PRESS),
          buttonId(id),
          pressed(isPressed) {}

    hal::ButtonID buttonId;
    bool pressed;
};

/// Button release event
class ButtonReleaseEvent : public Event {
public:
    explicit ButtonReleaseEvent(hal::ButtonID id)
        : Event(EventCategory::USER_INPUT, InputEvent::BUTTON_RELEASE), buttonId(id) {}

    hal::ButtonID buttonId;
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

/// Context was successfully registered
class ContextRegisteredEvent : public Event {
public:
    explicit ContextRegisteredEvent(const char* ctxId)
        : Event(EventCategory::CONTEXT, SystemEvent::CONTEXT_REGISTERED), contextId(ctxId) {}

    const char* contextId;
};

/// Context was activated (switched to)
class ContextActivatedEvent : public Event {
public:
    explicit ContextActivatedEvent(const char* ctxId)
        : Event(EventCategory::CONTEXT, SystemEvent::CONTEXT_ACTIVATED), contextId(ctxId) {}

    const char* contextId;
};

/// Context was deactivated (switched away from)
class ContextDeactivatedEvent : public Event {
public:
    explicit ContextDeactivatedEvent(const char* ctxId)
        : Event(EventCategory::CONTEXT, SystemEvent::CONTEXT_DEACTIVATED), contextId(ctxId) {}

    const char* contextId;
};

/// Context initialization failed
class ContextErrorEvent : public Event {
public:
    explicit ContextErrorEvent(const char* ctxId)
        : Event(EventCategory::CONTEXT, SystemEvent::CONTEXT_ERROR), contextId(ctxId) {}

    const char* contextId;
};

}  // namespace oc::core::event
