#include "TeensyUsbMidi.hpp"

#include <Arduino.h>

namespace oc::drivers::teensy {

// ═══════════════════════════════════════════════════
// Constructor
// ═══════════════════════════════════════════════════

TeensyUsbMidi::TeensyUsbMidi(const TeensyUsbMidiConfig& config)
    : max_active_notes_(config.maxActiveNotes) {}

// ═══════════════════════════════════════════════════
// Lifecycle
// ═══════════════════════════════════════════════════

bool TeensyUsbMidi::init() {
    if (initialized_) return true;

    // Reserve space for active notes tracking
    active_notes_.resize(max_active_notes_);
    for (auto& note : active_notes_) {
        note.active = false;
    }

    initialized_ = true;
    return true;
}

void TeensyUsbMidi::update() {
    if (!initialized_) return;

    // Read all pending MIDI messages (flush the buffer)
    while (usbMIDI.read()) {
        uint8_t type = usbMIDI.getType();
        uint8_t channel = usbMIDI.getChannel() - 1;  // Convert to 0-based
        uint8_t data1 = usbMIDI.getData1();
        uint8_t data2 = usbMIDI.getData2();

        switch (type) {
            case usbMIDI.ControlChange:
                if (on_cc_) on_cc_(channel, data1, data2);
                break;

            case usbMIDI.NoteOn:
                if (on_note_on_) on_note_on_(channel, data1, data2);
                break;

            case usbMIDI.NoteOff:
                if (on_note_off_) on_note_off_(channel, data1, data2);
                break;

            case usbMIDI.SystemExclusive:
                if (on_sysex_) {
                    const uint8_t* sysex = usbMIDI.getSysExArray();
                    size_t len = usbMIDI.getSysExArrayLength();
                    on_sysex_(sysex, len);
                }
                break;

            default:
                break;
        }
    }
}

// ═══════════════════════════════════════════════════
// Active Notes Tracking
// ═══════════════════════════════════════════════════

void TeensyUsbMidi::markNoteActive(uint8_t channel, uint8_t note) {
    // Find first inactive slot
    for (auto& slot : active_notes_) {
        if (!slot.active) {
            slot.channel = channel;
            slot.note = note;
            slot.active = true;
            return;
        }
    }
    // Buffer full - overwrite first slot (like Core)
    if (!active_notes_.empty()) {
        active_notes_[0].channel = channel;
        active_notes_[0].note = note;
        active_notes_[0].active = true;
    }
}

void TeensyUsbMidi::markNoteInactive(uint8_t channel, uint8_t note) {
    for (auto& slot : active_notes_) {
        if (slot.active && slot.channel == channel && slot.note == note) {
            slot.active = false;
            return;
        }
    }
}

// ═══════════════════════════════════════════════════
// Output
// ═══════════════════════════════════════════════════

void TeensyUsbMidi::sendCC(uint8_t channel, uint8_t cc, uint8_t value) {
    usbMIDI.sendControlChange(cc, value, channel + 1);
}

void TeensyUsbMidi::sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    markNoteActive(channel, note);
    usbMIDI.sendNoteOn(note, velocity, channel + 1);
}

void TeensyUsbMidi::sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) {
    markNoteInactive(channel, note);
    usbMIDI.sendNoteOff(note, velocity, channel + 1);
}

void TeensyUsbMidi::sendSysEx(const uint8_t* data, size_t length) {
    usbMIDI.sendSysEx(length, data, true);  // true = has F0/F7
}

void TeensyUsbMidi::sendProgramChange(uint8_t channel, uint8_t program) {
    usbMIDI.sendProgramChange(program, channel + 1);
}

void TeensyUsbMidi::sendPitchBend(uint8_t channel, int16_t value) {
    usbMIDI.sendPitchBend(value, channel + 1);
}

void TeensyUsbMidi::sendChannelPressure(uint8_t channel, uint8_t pressure) {
    usbMIDI.sendAfterTouch(pressure, channel + 1);
}

void TeensyUsbMidi::allNotesOff() {
    // Send NoteOff for all tracked active notes (like Core)
    for (auto& slot : active_notes_) {
        if (slot.active) {
            usbMIDI.sendNoteOff(slot.note, 0, slot.channel + 1);
            slot.active = false;
        }
    }
}

// ═══════════════════════════════════════════════════
// Input callbacks
// ═══════════════════════════════════════════════════

void TeensyUsbMidi::setOnCC(CCCallback cb) { on_cc_ = cb; }
void TeensyUsbMidi::setOnNoteOn(NoteCallback cb) { on_note_on_ = cb; }
void TeensyUsbMidi::setOnNoteOff(NoteCallback cb) { on_note_off_ = cb; }
void TeensyUsbMidi::setOnSysEx(SysExCallback cb) { on_sysex_ = cb; }

}  // namespace oc::drivers::teensy
