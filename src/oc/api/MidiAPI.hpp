#pragma once

#include <oc/interface/IMidi.hpp>

namespace oc::api {

/**
 * @brief API for MIDI input/output operations
 *
 * Provides:
 * - MIDI output (CC, Note, SysEx, Program Change, Pitch Bend, etc.)
 * - Safety operations (allNotesOff)
 *
 * @code
 * // Via IContext:
 * midi().sendCC(1, 20, 127);
 * midi().sendNoteOn(1, 60, 100);
 * @endcode
 */
class MidiAPI {
public:
    explicit MidiAPI(interface::IMidi& transport);

    // ═══════════════════════════════════════════════════
    // Output
    // ═══════════════════════════════════════════════════

    /// Send MIDI Control Change
    interface::MidiOutputAcceptance sendCC(uint8_t channel, uint8_t cc, uint8_t value);

    /// Send MIDI Note On
    interface::MidiOutputAcceptance sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity);

    /// Send MIDI Note Off
    interface::MidiOutputAcceptance sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity);

    /// Send MIDI System Exclusive message
    interface::MidiOutputAcceptance sendSysEx(const uint8_t* data, size_t length);

    /// Send MIDI Program Change
    interface::MidiOutputAcceptance sendProgramChange(uint8_t channel, uint8_t program);

    /// Send MIDI Pitch Bend
    interface::MidiOutputAcceptance sendPitchBend(uint8_t channel, int16_t value);

    /// Send MIDI Channel Pressure (Aftertouch)
    interface::MidiOutputAcceptance sendChannelPressure(uint8_t channel, uint8_t pressure);

    /// Send MIDI Clock realtime message (0xF8)
    interface::MidiOutputAcceptance sendClock();

    /// Send MIDI Start realtime message (0xFA)
    interface::MidiOutputAcceptance sendStart();

    /// Send MIDI Stop realtime message (0xFC)
    interface::MidiOutputAcceptance sendStop();

    /// Send MIDI Continue realtime message (0xFB)
    interface::MidiOutputAcceptance sendContinue();

    /// Drain buffered transport output when supported by the backend.
    void serviceOutput();
    void serviceOutput(uint32_t budgetUs);

    // ═══════════════════════════════════════════════════
    // Safety
    // ═══════════════════════════════════════════════════

    /**
     * @brief Stop all active notes
     *
     * Called automatically by ContextManager on context switch
     * to prevent hanging notes.
     */
    void allNotesOff();

private:
    interface::IMidi& transport_;
};

}  // namespace oc::api
