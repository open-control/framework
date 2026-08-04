#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

#include <oc/type/Result.hpp>

namespace oc::interface {

/**
 * @brief Immediate ownership result for one MIDI output message.
 *
 * ACCEPTED means the transport has taken ownership of the message, either by
 * sending it synchronously or by copying it into its own bounded buffer. It
 * does not mean the host/controller has completed physical delivery.
 * REJECTED means ownership remains with the caller and the message may be
 * retried without duplication.
 */
enum class MidiOutputAcceptance : uint8_t {
    REJECTED = 0,
    ACCEPTED,
};

/**
 * @brief Interface for MIDI I/O abstraction
 */
class IMidi {
public:
    virtual ~IMidi() = default;

    /**
     * @brief Initialize MIDI hardware
     * @return Result<void> - ok() on success, err() with ErrorCode on failure
     */
    virtual oc::type::Result<void> init() = 0;

    virtual void update() = 0;
    virtual void pollInput() { update(); }
    virtual void serviceOutput() {}
    virtual void serviceOutput(uint32_t budgetUs) {
        (void)budgetUs;
        serviceOutput();
    }

    // ═══════════════════════════════════════════════════
    // Output
    // ═══════════════════════════════════════════════════

    virtual MidiOutputAcceptance sendCC(uint8_t channel, uint8_t cc, uint8_t value) = 0;
    virtual MidiOutputAcceptance sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) = 0;
    virtual MidiOutputAcceptance sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) = 0;
    virtual MidiOutputAcceptance sendSysEx(const uint8_t* data, size_t length) = 0;
    virtual MidiOutputAcceptance sendProgramChange(uint8_t channel, uint8_t program) = 0;
    virtual MidiOutputAcceptance sendPitchBend(uint8_t channel, int16_t value) = 0;
    virtual MidiOutputAcceptance sendChannelPressure(uint8_t channel, uint8_t pressure) = 0;

    // MIDI realtime output
    virtual MidiOutputAcceptance sendClock() = 0;
    virtual MidiOutputAcceptance sendStart() = 0;
    virtual MidiOutputAcceptance sendStop() = 0;
    virtual MidiOutputAcceptance sendContinue() = 0;

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
    // Clock callback receives transport-captured timestamp (microseconds).
    using ClockCallback = std::function<void(uint64_t timestampUs)>;
    // Start/Continue/Stop callbacks are edge notifications.
    using RealtimeCallback = std::function<void()>;

    virtual void setOnCC(CCCallback cb) = 0;
    virtual void setOnNoteOn(NoteCallback cb) = 0;
    virtual void setOnNoteOff(NoteCallback cb) = 0;
    virtual void setOnSysEx(SysExCallback cb) = 0;

    // MIDI realtime callbacks
    virtual void setOnClock(ClockCallback cb) = 0;
    virtual void setOnStart(RealtimeCallback cb) = 0;
    virtual void setOnStop(RealtimeCallback cb) = 0;
    virtual void setOnContinue(RealtimeCallback cb) = 0;
};

}  // namespace oc::interface
