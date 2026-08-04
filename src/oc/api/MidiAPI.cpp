#include "MidiAPI.hpp"

#include <oc/log/Log.hpp>

namespace oc::api {

namespace {

/// Validate MIDI channel (0-15 for USB MIDI convention)
bool validateChannel(uint8_t channel) {
    if (channel > 15) {
        OC_LOG_WARN("{}", "[MidiAPI] Invalid channel (expected 0-15)");
        return false;
    }
    return true;
}

/// Validate 7-bit MIDI value (0-127)
bool validate7bit(uint8_t value, const char* name) {
    if (value > 127) {
        OC_LOG_WARN("{}", "[MidiAPI] Invalid value (expected 0-127)");
        return false;
    }
    return true;
}

/// Validate pitch bend range (-8192 to 8191)
bool validatePitchBend(int16_t value) {
    if (value < -8192 || value > 8191) {
        OC_LOG_WARN("{}", "[MidiAPI] Invalid pitch bend (expected -8192 to 8191)");
        return false;
    }
    return true;
}

}  // namespace

MidiAPI::MidiAPI(interface::IMidi& transport) : transport_(transport) {}

// ═══════════════════════════════════════════════════
// Output
// ═══════════════════════════════════════════════════

interface::MidiOutputAcceptance MidiAPI::sendCC(uint8_t channel, uint8_t cc, uint8_t value) {
    if (!validateChannel(channel) || !validate7bit(cc, "cc") || !validate7bit(value, "value")) {
        return interface::MidiOutputAcceptance::REJECTED;
    }
    return transport_.sendCC(channel, cc, value);
}

interface::MidiOutputAcceptance MidiAPI::sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    if (!validateChannel(channel) || !validate7bit(note, "note") || !validate7bit(velocity, "velocity")) {
        return interface::MidiOutputAcceptance::REJECTED;
    }
    return transport_.sendNoteOn(channel, note, velocity);
}

interface::MidiOutputAcceptance MidiAPI::sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) {
    if (!validateChannel(channel) || !validate7bit(note, "note") || !validate7bit(velocity, "velocity")) {
        return interface::MidiOutputAcceptance::REJECTED;
    }
    return transport_.sendNoteOff(channel, note, velocity);
}

interface::MidiOutputAcceptance MidiAPI::sendSysEx(const uint8_t* data, size_t length) {
    if (!data || length == 0) {
        OC_LOG_WARN("{}", "[MidiAPI] Invalid SysEx data");
        return interface::MidiOutputAcceptance::REJECTED;
    }
    return transport_.sendSysEx(data, length);
}

interface::MidiOutputAcceptance MidiAPI::sendProgramChange(uint8_t channel, uint8_t program) {
    if (!validateChannel(channel) || !validate7bit(program, "program")) {
        return interface::MidiOutputAcceptance::REJECTED;
    }
    return transport_.sendProgramChange(channel, program);
}

interface::MidiOutputAcceptance MidiAPI::sendPitchBend(uint8_t channel, int16_t value) {
    if (!validateChannel(channel) || !validatePitchBend(value)) {
        return interface::MidiOutputAcceptance::REJECTED;
    }
    return transport_.sendPitchBend(channel, value);
}

interface::MidiOutputAcceptance MidiAPI::sendChannelPressure(uint8_t channel, uint8_t pressure) {
    if (!validateChannel(channel) || !validate7bit(pressure, "pressure")) {
        return interface::MidiOutputAcceptance::REJECTED;
    }
    return transport_.sendChannelPressure(channel, pressure);
}

interface::MidiOutputAcceptance MidiAPI::sendClock() {
    return transport_.sendClock();
}

interface::MidiOutputAcceptance MidiAPI::sendStart() {
    return transport_.sendStart();
}

interface::MidiOutputAcceptance MidiAPI::sendStop() {
    return transport_.sendStop();
}

interface::MidiOutputAcceptance MidiAPI::sendContinue() {
    return transport_.sendContinue();
}

void MidiAPI::serviceOutput() {
    transport_.serviceOutput();
}

void MidiAPI::serviceOutput(uint32_t budgetUs) {
    transport_.serviceOutput(budgetUs);
}

// ═══════════════════════════════════════════════════
// Safety
// ═══════════════════════════════════════════════════

void MidiAPI::allNotesOff() {
    transport_.allNotesOff();
}

}  // namespace oc::api
