#pragma once

#include <cstdint>

#include <oc/type/Event.hpp>
#include "EventTypes.hpp"

#include <oc/type/Ids.hpp>
#include <oc/type/Callbacks.hpp>

namespace oc::core::event {

/// Encoder rotation event
class EncoderChangedEvent : public oc::type::Event {
public:
    EncoderChangedEvent(oc::type::EncoderID id, float value)
        : oc::type::Event(oc::type::EventCategory::USER_INPUT, InputEvent::ENCODER_CHANGED),
          encoderId(id),
          normalizedValue(value) {}

    oc::type::EncoderID encoderId;
    float normalizedValue;
};

/// Button press event
class ButtonPressEvent : public oc::type::Event {
public:
    ButtonPressEvent(oc::type::ButtonID id, bool isPressed)
        : oc::type::Event(oc::type::EventCategory::USER_INPUT, InputEvent::BUTTON_PRESS),
          buttonId(id),
          pressed(isPressed) {}

    oc::type::ButtonID buttonId;
    bool pressed;
};

/// Button release event
class ButtonReleaseEvent : public oc::type::Event {
public:
    explicit ButtonReleaseEvent(oc::type::ButtonID id)
        : oc::type::Event(oc::type::EventCategory::USER_INPUT, InputEvent::BUTTON_RELEASE), buttonId(id) {}

    oc::type::ButtonID buttonId;
};

/// MIDI Control Change event
class MidiCCEvent : public oc::type::Event {
public:
    MidiCCEvent(uint8_t ch, uint8_t cc, uint8_t val)
        : oc::type::Event(oc::type::EventCategory::MIDI, MidiEvent::CC),
          channel(ch),
          controller(cc),
          value(val) {}

    uint8_t channel;
    uint8_t controller;
    uint8_t value;
};

/// MIDI Note On event
class MidiNoteOnEvent : public oc::type::Event {
public:
    MidiNoteOnEvent(uint8_t ch, uint8_t n, uint8_t vel)
        : oc::type::Event(oc::type::EventCategory::MIDI, MidiEvent::NOTE_ON),
          channel(ch),
          note(n),
          velocity(vel) {}

    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
};

/// MIDI Note Off event
class MidiNoteOffEvent : public oc::type::Event {
public:
    MidiNoteOffEvent(uint8_t ch, uint8_t n, uint8_t vel)
        : oc::type::Event(oc::type::EventCategory::MIDI, MidiEvent::NOTE_OFF),
          channel(ch),
          note(n),
          velocity(vel) {}

    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
};

/// MIDI System Exclusive event
class SysExEvent : public oc::type::Event {
public:
    SysExEvent(const uint8_t* d, uint16_t len)
        : oc::type::Event(oc::type::EventCategory::MIDI, MidiEvent::SYSEX), data(d), length(len) {}

    const uint8_t* data;
    uint16_t length;
};

/// MIDI Clock tick (24 PPQN)
class MidiClockEvent : public oc::type::Event {
public:
    explicit MidiClockEvent(uint64_t tsUs)
        : oc::type::Event(oc::type::EventCategory::MIDI, MidiEvent::CLOCK)
        , timestampUs(tsUs) {}

    uint64_t timestampUs;
};

/// MIDI realtime Start
class MidiStartEvent : public oc::type::Event {
public:
    MidiStartEvent() : oc::type::Event(oc::type::EventCategory::MIDI, MidiEvent::START) {}
};

/// MIDI realtime Continue
class MidiContinueEvent : public oc::type::Event {
public:
    MidiContinueEvent() : oc::type::Event(oc::type::EventCategory::MIDI, MidiEvent::CONTINUE) {}
};

/// MIDI realtime Stop
class MidiStopEvent : public oc::type::Event {
public:
    MidiStopEvent() : oc::type::Event(oc::type::EventCategory::MIDI, MidiEvent::STOP) {}
};

/// System boot complete event
class SystemBootCompleteEvent : public oc::type::Event {
public:
    SystemBootCompleteEvent() : oc::type::Event(oc::type::EventCategory::SYSTEM, SystemEvent::BOOT_COMPLETE) {}
};

// ═══════════════════════════════════════════════════════════════════
// Context Events
// ═══════════════════════════════════════════════════════════════════

/// Context was activated (switched to)
class ContextActivatedEvent : public oc::type::Event {
public:
    ContextActivatedEvent(uint8_t id, const char* name)
        : oc::type::Event(oc::type::EventCategory::CONTEXT, SystemEvent::CONTEXT_ACTIVATED),
          contextId(id),
          contextName(name) {}

    uint8_t contextId;       ///< Numeric ID for logic
    const char* contextName; ///< Human-readable name for logging
};

/// Context was deactivated (switched away from)
class ContextDeactivatedEvent : public oc::type::Event {
public:
    ContextDeactivatedEvent(uint8_t id, const char* name)
        : oc::type::Event(oc::type::EventCategory::CONTEXT, SystemEvent::CONTEXT_DEACTIVATED),
          contextId(id),
          contextName(name) {}

    uint8_t contextId;
    const char* contextName;
};

/// Context initialization failed
class ContextErrorEvent : public oc::type::Event {
public:
    explicit ContextErrorEvent(uint8_t id)
        : oc::type::Event(oc::type::EventCategory::CONTEXT, SystemEvent::CONTEXT_ERROR), contextId(id) {}

    uint8_t contextId;
};

}  // namespace oc::core::event
