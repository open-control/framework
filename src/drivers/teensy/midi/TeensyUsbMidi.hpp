#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <oc/hal/IMidiTransport.hpp>

namespace oc::drivers::teensy {

/**
 * @brief Configuration for TeensyUsbMidi
 */
struct TeensyUsbMidiConfig {
    size_t maxActiveNotes = 32;  ///< Maximum simultaneous active notes to track
};

/**
 * @brief Teensy USB MIDI driver
 *
 * Implements IMidiTransport using Teensy's native USB MIDI.
 * Requires USB Type "MIDI" or "Serial + MIDI" in platformio.ini.
 *
 * Tracks active notes to support allNotesOff() properly.
 *
 * @note Teensy USB MIDI uses 1-based channels internally.
 *       This driver handles the conversion (HAL uses 0-based).
 */
class TeensyUsbMidi : public hal::IMidiTransport {
public:
    static constexpr size_t DEFAULT_MAX_ACTIVE_NOTES = 32;

    TeensyUsbMidi() = default;
    explicit TeensyUsbMidi(const TeensyUsbMidiConfig& config);
    ~TeensyUsbMidi() override = default;

    // Non-copyable
    TeensyUsbMidi(const TeensyUsbMidi&) = delete;
    TeensyUsbMidi& operator=(const TeensyUsbMidi&) = delete;

    bool init() override;
    void update() override;

    // ═══════════════════════════════════════════════════
    // Output
    // ═══════════════════════════════════════════════════

    void sendCC(uint8_t channel, uint8_t cc, uint8_t value) override;
    void sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) override;
    void sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) override;
    void sendSysEx(const uint8_t* data, size_t length) override;
    void sendProgramChange(uint8_t channel, uint8_t program) override;
    void sendPitchBend(uint8_t channel, int16_t value) override;
    void sendChannelPressure(uint8_t channel, uint8_t pressure) override;
    void allNotesOff() override;

    // ═══════════════════════════════════════════════════
    // Input callbacks
    // ═══════════════════════════════════════════════════

    void setOnCC(CCCallback cb) override;
    void setOnNoteOn(NoteCallback cb) override;
    void setOnNoteOff(NoteCallback cb) override;
    void setOnSysEx(SysExCallback cb) override;

private:
    struct ActiveNote {
        uint8_t channel;
        uint8_t note;
        bool active;
    };

    void markNoteActive(uint8_t channel, uint8_t note);
    void markNoteInactive(uint8_t channel, uint8_t note);

    CCCallback on_cc_;
    NoteCallback on_note_on_;
    NoteCallback on_note_off_;
    SysExCallback on_sysex_;

    std::vector<ActiveNote> active_notes_;
    size_t max_active_notes_ = DEFAULT_MAX_ACTIVE_NOTES;
    bool initialized_ = false;
};

}  // namespace oc::drivers::teensy
