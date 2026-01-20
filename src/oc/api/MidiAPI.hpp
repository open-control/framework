#pragma once

#include <oc/interface/IMidi.hpp>

namespace oc::api {

/**
 * @brief API for MIDI input/output operations
 *
 * Provides:
 * - MIDI output (CC, Note, SysEx, Program Change, Pitch Bend, etc.)
 * - MIDI input callbacks
 * - Safety operations (allNotesOff)
 *
 * @code
 * // Via IContext:
 * midi().sendCC(1, 20, 127);
 * midi().sendNoteOn(1, 60, 100);
 * midi().onCC([](uint8_t ch, uint8_t cc, uint8_t val) { ... });
 * @endcode
 */
class MidiAPI {
public:
    explicit MidiAPI(interface::IMidi& transport);

    // ═══════════════════════════════════════════════════
    // Output
    // ═══════════════════════════════════════════════════

    /// Send MIDI Control Change
    void sendCC(uint8_t channel, uint8_t cc, uint8_t value);

    /// Send MIDI Note On
    void sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity);

    /// Send MIDI Note Off
    void sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity);

    /// Send MIDI System Exclusive message
    void sendSysEx(const uint8_t* data, size_t length);

    /// Send MIDI Program Change
    void sendProgramChange(uint8_t channel, uint8_t program);

    /// Send MIDI Pitch Bend
    void sendPitchBend(uint8_t channel, int16_t value);

    /// Send MIDI Channel Pressure (Aftertouch)
    void sendChannelPressure(uint8_t channel, uint8_t pressure);

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

    // ═══════════════════════════════════════════════════
    // Input callbacks
    // ═══════════════════════════════════════════════════

    /// Set callback for incoming CC messages
    void onCC(interface::IMidi::CCCallback cb);

    /// Set callback for incoming Note On messages
    void onNoteOn(interface::IMidi::NoteCallback cb);

    /// Set callback for incoming Note Off messages
    void onNoteOff(interface::IMidi::NoteCallback cb);

    /// Set callback for incoming SysEx messages
    void onSysEx(interface::IMidi::SysExCallback cb);

private:
    interface::IMidi& transport_;
};

}  // namespace oc::api
