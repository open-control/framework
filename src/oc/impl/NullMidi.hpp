#pragma once

#include <oc/interface/IMidi.hpp>
#include <oc/type/Result.hpp>

namespace oc::impl {

/**
 * @brief No-op MIDI transport for testing and platforms without MIDI hardware
 *
 * All operations succeed silently. Use cases:
 * - Desktop development (SDL) without MIDI hardware
 * - Unit tests requiring MidiAPI but not actual MIDI
 * - Contexts with REQUIRES.midi=true on platforms without MIDI
 *
 * @note For platforms that simply don't need MIDI, prefer not configuring
 * midi in AppBuilder rather than using this class.
 */
class NullMidi : public interface::IMidi {
public:
    using MidiOutputAcceptance = interface::MidiOutputAcceptance;

    oc::type::Result<void> init() override {
        return oc::type::Result<void>::ok();
    }

    void update() override {}

    // Output (all no-op)
    MidiOutputAcceptance sendCC(uint8_t, uint8_t, uint8_t) override {
        return MidiOutputAcceptance::ACCEPTED;
    }
    MidiOutputAcceptance sendNoteOn(uint8_t, uint8_t, uint8_t) override {
        return MidiOutputAcceptance::ACCEPTED;
    }
    MidiOutputAcceptance sendNoteOff(uint8_t, uint8_t, uint8_t) override {
        return MidiOutputAcceptance::ACCEPTED;
    }
    MidiOutputAcceptance sendSysEx(const uint8_t*, size_t) override {
        return MidiOutputAcceptance::ACCEPTED;
    }
    MidiOutputAcceptance sendProgramChange(uint8_t, uint8_t) override {
        return MidiOutputAcceptance::ACCEPTED;
    }
    MidiOutputAcceptance sendPitchBend(uint8_t, int16_t) override {
        return MidiOutputAcceptance::ACCEPTED;
    }
    MidiOutputAcceptance sendChannelPressure(uint8_t, uint8_t) override {
        return MidiOutputAcceptance::ACCEPTED;
    }
    MidiOutputAcceptance sendClock() override {
        return MidiOutputAcceptance::ACCEPTED;
    }
    MidiOutputAcceptance sendStart() override {
        return MidiOutputAcceptance::ACCEPTED;
    }
    MidiOutputAcceptance sendStop() override {
        return MidiOutputAcceptance::ACCEPTED;
    }
    MidiOutputAcceptance sendContinue() override {
        return MidiOutputAcceptance::ACCEPTED;
    }
    void allNotesOff() override {}

    // Input callbacks (all no-op)
    void setOnCC(CCCallback) override {}
    void setOnNoteOn(NoteCallback) override {}
    void setOnNoteOff(NoteCallback) override {}
    void setOnSysEx(SysExCallback) override {}
    void setOnClock(interface::IMidi::ClockCallback) override {}
    void setOnStart(RealtimeCallback) override {}
    void setOnStop(RealtimeCallback) override {}
    void setOnContinue(RealtimeCallback) override {}
};

}  // namespace oc::impl
