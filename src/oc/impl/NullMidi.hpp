#pragma once

#include <oc/interface/IMidi.hpp>
#include <oc/types/Result.hpp>

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
    oc::Result<void> init() override {
        return oc::Result<void>::ok();
    }

    void update() override {}

    // Output (all no-op)
    void sendCC(uint8_t, uint8_t, uint8_t) override {}
    void sendNoteOn(uint8_t, uint8_t, uint8_t) override {}
    void sendNoteOff(uint8_t, uint8_t, uint8_t) override {}
    void sendSysEx(const uint8_t*, size_t) override {}
    void sendProgramChange(uint8_t, uint8_t) override {}
    void sendPitchBend(uint8_t, int16_t) override {}
    void sendChannelPressure(uint8_t, uint8_t) override {}
    void allNotesOff() override {}

    // Input callbacks (all no-op)
    void setOnCC(CCCallback) override {}
    void setOnNoteOn(NoteCallback) override {}
    void setOnNoteOff(NoteCallback) override {}
    void setOnSysEx(SysExCallback) override {}
};

}  // namespace oc::impl
