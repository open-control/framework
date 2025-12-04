#pragma once

#include <cstddef>
#include <cstdint>

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
 *     api.onPressed(BTN_1, []() { doAction(); });
 *     api.onTurned(ENC_1, [](float v) { setParam(v); });
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
    // Input Binding - Global (always active)
    // ═══════════════════════════════════════════════════

    /// Register callback for button press
    void onPressed(hal::ButtonID id, core::ActionCallback cb);

    /// Register callback for button release
    void onReleased(hal::ButtonID id, core::ActionCallback cb);

    /// Register callback for long press (0 ms = use config default)
    void onLongPress(hal::ButtonID id, core::ActionCallback cb, uint32_t ms = 0);

    /// Register callback for double tap
    void onDoubleTap(hal::ButtonID id, core::ActionCallback cb);

    /// Register callback for two-button combo
    void onCombo(hal::ButtonID btn1, hal::ButtonID btn2, core::ActionCallback cb);

    /// Register callback for encoder rotation
    void onTurned(hal::EncoderID id, core::EncoderActionCallback cb);

    /// Register callback for encoder rotation while button held
    void onTurnedWhilePressed(hal::EncoderID enc, hal::ButtonID btn, core::EncoderActionCallback cb);

    // ═══════════════════════════════════════════════════
    // Input Binding - Scoped (active when scope visible)
    // ═══════════════════════════════════════════════════

    /// Register scoped button press callback
    void onPressed(hal::ButtonID id, core::ActionCallback cb, core::VisibilityPredicate isVisible,
                   core::ScopeId scope, bool latch = false);

    /// Register scoped button release callback
    void onReleased(hal::ButtonID id, core::ActionCallback cb, core::VisibilityPredicate isVisible,
                    core::ScopeId scope);

    /// Register scoped long press callback
    void onLongPress(hal::ButtonID id, core::ActionCallback cb, uint32_t ms,
                     core::VisibilityPredicate isVisible, core::ScopeId scope);

    /// Register scoped double tap callback
    void onDoubleTap(hal::ButtonID id, core::ActionCallback cb, core::VisibilityPredicate isVisible,
                     core::ScopeId scope);

    /// Register scoped two-button combo callback
    void onCombo(hal::ButtonID btn1, hal::ButtonID btn2, core::ActionCallback cb,
                 core::VisibilityPredicate isVisible, core::ScopeId scope);

    /// Register scoped encoder rotation callback
    void onTurned(hal::EncoderID id, core::EncoderActionCallback cb,
                  core::VisibilityPredicate isVisible, core::ScopeId scope);

    /// Register scoped encoder rotation while button held callback
    void onTurnedWhilePressed(hal::EncoderID enc, hal::ButtonID btn, core::EncoderActionCallback cb,
                              core::VisibilityPredicate isVisible, core::ScopeId scope);

    /// Remove all bindings for a scope
    void clearScope(core::ScopeId scope);

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

    /// Get current encoder position
    int32_t getEncoderPosition(hal::EncoderID id) const;

    /// Set encoder position
    void setEncoderPosition(hal::EncoderID id, int32_t position);

    /// Set encoder operating mode
    void setEncoderMode(hal::EncoderID id, hal::EncoderMode mode);

    /// Set encoder bounds for absolute mode
    void setEncoderBounds(hal::EncoderID id, int32_t min, int32_t max);

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
