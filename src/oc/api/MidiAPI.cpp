#include "MidiAPI.hpp"

namespace oc::api {

MidiAPI::MidiAPI(hal::IMidiTransport& transport) : transport_(transport) {}

// ═══════════════════════════════════════════════════
// Output
// ═══════════════════════════════════════════════════

void MidiAPI::sendCC(uint8_t channel, uint8_t cc, uint8_t value) {
    transport_.sendCC(channel, cc, value);
}

void MidiAPI::sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    transport_.sendNoteOn(channel, note, velocity);
}

void MidiAPI::sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) {
    transport_.sendNoteOff(channel, note, velocity);
}

void MidiAPI::sendSysEx(const uint8_t* data, size_t length) {
    transport_.sendSysEx(data, length);
}

void MidiAPI::sendProgramChange(uint8_t channel, uint8_t program) {
    transport_.sendProgramChange(channel, program);
}

void MidiAPI::sendPitchBend(uint8_t channel, int16_t value) {
    transport_.sendPitchBend(channel, value);
}

void MidiAPI::sendChannelPressure(uint8_t channel, uint8_t pressure) {
    transport_.sendChannelPressure(channel, pressure);
}

// ═══════════════════════════════════════════════════
// Safety
// ═══════════════════════════════════════════════════

void MidiAPI::allNotesOff() {
    transport_.allNotesOff();
}

// ═══════════════════════════════════════════════════
// Input callbacks
// ═══════════════════════════════════════════════════

void MidiAPI::onCC(hal::IMidiTransport::CCCallback cb) {
    transport_.setOnCC(std::move(cb));
}

void MidiAPI::onNoteOn(hal::IMidiTransport::NoteCallback cb) {
    transport_.setOnNoteOn(std::move(cb));
}

void MidiAPI::onNoteOff(hal::IMidiTransport::NoteCallback cb) {
    transport_.setOnNoteOff(std::move(cb));
}

void MidiAPI::onSysEx(hal::IMidiTransport::SysExCallback cb) {
    transport_.setOnSysEx(std::move(cb));
}

}  // namespace oc::api
