#pragma once

#include <cstddef>
#include <cstdint>

#include <oc/core/input/ButtonBuilder.hpp>
#include <oc/core/input/EncoderBuilder.hpp>
#include <oc/core/struct/Binding.hpp>
#include <oc/hal/IEncoderController.hpp>
#include <oc/hal/IMidiTransport.hpp>
#include <oc/hal/Types.hpp>

namespace oc::core::input {
class InputBinding;
}

namespace oc::core::event {
class IEventBus;
}

namespace oc::api {

/**
 * @brief Facade API for context implementations
 *
 * Provides a unified interface for contexts to interact with hardware
 * through InputBinding (buttons, encoders), MIDI transport, and encoder
 * configuration. Contexts receive a ControlAPI reference during initialize().
 *
 * @code
 * bool MyContext::initialize(ControlAPI& api) {
 *     api.button(BTN_1).onPress().then([]{ doAction(); });
 *     api.encoder(ENC_1).onTurn().then([](float v){ setParam(v); });
 *     api.sendCC(0, 1, 127);
 *     return true;
 * }
 * @endcode
 */
class ControlAPI {
public:
    /**
     * @brief Construct ControlAPI with all required dependencies
     * @param binding Reference to InputBinding for button/encoder handling
     * @param eventBus Reference to EventBus for event emission
     * @param midi Reference to MIDI transport for I/O
     * @param encoders Reference to encoder controller for configuration
     */
    ControlAPI(core::input::InputBinding& binding, core::event::IEventBus& eventBus,
               hal::IMidiTransport& midi, hal::IEncoderController& encoders);

    ~ControlAPI() = default;

    ControlAPI(const ControlAPI&) = delete;
    ControlAPI& operator=(const ControlAPI&) = delete;

    // ═══════════════════════════════════════════════════
    // Input Binding - Fluent API
    // ═══════════════════════════════════════════════════

    /**
     * @brief Start building a button binding
     *
     * @code
     * api.button(BTN_1).onPress().then([]{ doAction(); });
     * api.button(BTN_1).onLongPress(800).scope(view).then([]{ showMenu(); });
     * api.button(BTN_1).combo(BTN_2).then([]{ resetAll(); });
     * @endcode
     *
     * @param id The button to bind
     * @return ButtonBuilder for chained configuration
     */
    [[nodiscard]] core::input::ButtonBuilder button(hal::ButtonID id);

    /**
     * @brief Start building an encoder binding
     *
     * @code
     * api.encoder(ENC_1).onTurn().then([](float v){ setValue(v); });
     * api.encoder(ENC_1).onTurnWhilePressed(BTN_SHIFT).then([](float v){ fineAdjust(v); });
     * @endcode
     *
     * @param id The encoder to bind
     * @return EncoderBuilder for chained configuration
     */
    [[nodiscard]] core::input::EncoderBuilder encoder(hal::EncoderID id);

    /**
     * @brief Remove all bindings for a scope
     * @param scope The scope ID to clear
     */
    void clearScope(core::ScopeID scope);

    // ═══════════════════════════════════════════════════
    // Latch State
    // ═══════════════════════════════════════════════════

    /// Check if a button is in latched state
    bool isLatched(hal::ButtonID btn) const;

    /// Set button latch state
    void setLatch(hal::ButtonID btn, bool latched);

    // ═══════════════════════════════════════════════════
    // Encoder Control
    // ═══════════════════════════════════════════════════

    /// Get current encoder value (depends on mode: NORMALIZED/RAW/RELATIVE)
    float getEncoderPosition(hal::EncoderID id) const;

    /// Set encoder value (depends on mode)
    void setEncoderPosition(hal::EncoderID id, float value);

    /// Set encoder operating mode
    void setEncoderMode(hal::EncoderID id, hal::EncoderMode mode);

    /// Set encoder bounds for absolute mode
    void setEncoderBounds(hal::EncoderID id, float min, float max);

    /// Set delta per detent for relative mode
    void setEncoderDelta(hal::EncoderID id, float delta);

    /// Configure encoder for discrete steps
    void setEncoderDiscreteSteps(hal::EncoderID id, uint8_t steps);

    /// Configure encoder for continuous mode
    void setEncoderContinuous(hal::EncoderID id);

    // ═══════════════════════════════════════════════════
    // MIDI Output
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

    // ═══════════════════════════════════════════════════
    // MIDI Input
    // ═══════════════════════════════════════════════════

    /// Set callback for incoming CC messages
    void onCC(hal::IMidiTransport::CCCallback cb);

    /// Set callback for incoming Note On messages
    void onNoteOn(hal::IMidiTransport::NoteCallback cb);

    /// Set callback for incoming Note Off messages
    void onNoteOff(hal::IMidiTransport::NoteCallback cb);

    /// Set callback for incoming SysEx messages
    void onSysEx(hal::IMidiTransport::SysExCallback cb);

    // ═══════════════════════════════════════════════════
    // Direct Access (for advanced use)
    // ═══════════════════════════════════════════════════

    /// Get reference to underlying event bus
    core::event::IEventBus& eventBus() { return eventBus_; }

private:
    core::input::InputBinding& binding_;
    core::event::IEventBus& eventBus_;
    hal::IMidiTransport& midi_;
    hal::IEncoderController& encoders_;
};

}  // namespace oc::api
