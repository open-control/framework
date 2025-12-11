#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

#include <oc/core/Result.hpp>

namespace oc::hal {

/**
 * @brief Interface for MIDI I/O abstraction
 */
class IMidiTransport {
public:
    virtual ~IMidiTransport() = default;

    /**
     * @brief Initialize MIDI hardware
     * @return Result<void> - ok() on success, err() with ErrorCode on failure
     */
    virtual core::Result<void> init() = 0;

    virtual void update() = 0;

    // ═══════════════════════════════════════════════════
    // Output
    // ═══════════════════════════════════════════════════

    virtual void sendCC(uint8_t channel, uint8_t cc, uint8_t value) = 0;
    virtual void sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) = 0;
    virtual void sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) = 0;
    virtual void sendSysEx(const uint8_t* data, size_t length) = 0;
    virtual void sendProgramChange(uint8_t channel, uint8_t program) = 0;
    virtual void sendPitchBend(uint8_t channel, int16_t value) = 0;
    virtual void sendChannelPressure(uint8_t channel, uint8_t pressure) = 0;

    /**
     * @brief Stop all active notes
     *
     * Called during context switch to prevent hanging notes.
     * Implementation may track active notes or send All Notes Off CC.
     */
    virtual void allNotesOff() {}

    // ═══════════════════════════════════════════════════
    // Input callbacks
    // ═══════════════════════════════════════════════════

    using CCCallback = std::function<void(uint8_t ch, uint8_t cc, uint8_t val)>;
    using NoteCallback = std::function<void(uint8_t ch, uint8_t note, uint8_t vel)>;
    using SysExCallback = std::function<void(const uint8_t* data, size_t len)>;

    virtual void setOnCC(CCCallback cb) = 0;
    virtual void setOnNoteOn(NoteCallback cb) = 0;
    virtual void setOnNoteOff(NoteCallback cb) = 0;
    virtual void setOnSysEx(SysExCallback cb) = 0;
};

}  // namespace oc::hal
