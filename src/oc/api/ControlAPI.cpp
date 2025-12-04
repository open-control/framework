#include "ControlAPI.hpp"

#include <oc/core/input/InputBinding.hpp>

namespace oc::api {

ControlAPI::ControlAPI(core::input::InputBinding& binding, core::event::IEventBus& eventBus,
                       hal::IMidiTransport& midi, hal::IEncoderController& encoders)
    : binding_(binding), eventBus_(eventBus), midi_(midi), encoders_(encoders) {}

// ═══════════════════════════════════════════════════
// Input Binding - Global
// ═══════════════════════════════════════════════════

void ControlAPI::onPressed(hal::ButtonID id, core::ActionCallback cb) {
    binding_.onPressed(id, std::move(cb));
}

void ControlAPI::onReleased(hal::ButtonID id, core::ActionCallback cb) {
    binding_.onReleased(id, std::move(cb));
}

void ControlAPI::onLongPress(hal::ButtonID id, core::ActionCallback cb, uint32_t ms) {
    binding_.onLongPress(id, std::move(cb), ms);
}

void ControlAPI::onDoubleTap(hal::ButtonID id, core::ActionCallback cb) {
    binding_.onDoubleTap(id, std::move(cb));
}

void ControlAPI::onCombo(hal::ButtonID btn1, hal::ButtonID btn2, core::ActionCallback cb) {
    binding_.onCombo(btn1, btn2, std::move(cb));
}

void ControlAPI::onTurned(hal::EncoderID id, core::EncoderActionCallback cb) {
    binding_.onTurned(id, std::move(cb));
}

void ControlAPI::onTurnedWhilePressed(hal::EncoderID enc, hal::ButtonID btn,
                                      core::EncoderActionCallback cb) {
    binding_.onTurnedWhilePressed(enc, btn, std::move(cb));
}

// ═══════════════════════════════════════════════════
// Input Binding - Scoped
// ═══════════════════════════════════════════════════

void ControlAPI::onPressed(hal::ButtonID id, core::ActionCallback cb,
                           core::VisibilityPredicate isVisible, core::ScopeId scope, bool latch) {
    binding_.onPressed(id, std::move(cb), std::move(isVisible), scope, latch);
}

void ControlAPI::onReleased(hal::ButtonID id, core::ActionCallback cb,
                            core::VisibilityPredicate isVisible, core::ScopeId scope) {
    binding_.onReleased(id, std::move(cb), std::move(isVisible), scope);
}

void ControlAPI::onLongPress(hal::ButtonID id, core::ActionCallback cb, uint32_t ms,
                             core::VisibilityPredicate isVisible, core::ScopeId scope) {
    binding_.onLongPress(id, std::move(cb), ms, std::move(isVisible), scope);
}

void ControlAPI::onDoubleTap(hal::ButtonID id, core::ActionCallback cb,
                             core::VisibilityPredicate isVisible, core::ScopeId scope) {
    binding_.onDoubleTap(id, std::move(cb), std::move(isVisible), scope);
}

void ControlAPI::onCombo(hal::ButtonID btn1, hal::ButtonID btn2, core::ActionCallback cb,
                         core::VisibilityPredicate isVisible, core::ScopeId scope) {
    binding_.onCombo(btn1, btn2, std::move(cb), std::move(isVisible), scope);
}

void ControlAPI::onTurned(hal::EncoderID id, core::EncoderActionCallback cb,
                          core::VisibilityPredicate isVisible, core::ScopeId scope) {
    binding_.onTurned(id, std::move(cb), std::move(isVisible), scope);
}

void ControlAPI::onTurnedWhilePressed(hal::EncoderID enc, hal::ButtonID btn,
                                      core::EncoderActionCallback cb,
                                      core::VisibilityPredicate isVisible, core::ScopeId scope) {
    binding_.onTurnedWhilePressed(enc, btn, std::move(cb), std::move(isVisible), scope);
}

void ControlAPI::clearScope(core::ScopeId scope) {
    binding_.clearScope(scope);
}

// ═══════════════════════════════════════════════════
// Latch State
// ═══════════════════════════════════════════════════

bool ControlAPI::isLatched(hal::ButtonID btn) const {
    return binding_.isLatched(btn);
}

void ControlAPI::setLatch(hal::ButtonID btn, bool latched) {
    binding_.setLatch(btn, latched);
}

// ═══════════════════════════════════════════════════
// Encoder Control
// ═══════════════════════════════════════════════════

int32_t ControlAPI::getEncoderPosition(hal::EncoderID id) const {
    return encoders_.getPosition(id);
}

void ControlAPI::setEncoderPosition(hal::EncoderID id, int32_t position) {
    encoders_.setPosition(id, position);
}

void ControlAPI::setEncoderMode(hal::EncoderID id, hal::EncoderMode mode) {
    encoders_.setMode(id, mode);
}

void ControlAPI::setEncoderBounds(hal::EncoderID id, int32_t min, int32_t max) {
    encoders_.setBounds(id, min, max);
}

void ControlAPI::setEncoderDelta(hal::EncoderID id, float delta) {
    encoders_.setDelta(id, delta);
}

void ControlAPI::setEncoderDiscreteSteps(hal::EncoderID id, uint8_t steps) {
    encoders_.setDiscreteSteps(id, steps);
}

void ControlAPI::setEncoderContinuous(hal::EncoderID id) {
    encoders_.setContinuous(id);
}

// ═══════════════════════════════════════════════════
// MIDI Output
// ═══════════════════════════════════════════════════

void ControlAPI::sendCC(uint8_t channel, uint8_t cc, uint8_t value) {
    midi_.sendCC(channel, cc, value);
}

void ControlAPI::sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    midi_.sendNoteOn(channel, note, velocity);
}

void ControlAPI::sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) {
    midi_.sendNoteOff(channel, note, velocity);
}

void ControlAPI::sendSysEx(const uint8_t* data, size_t length) {
    midi_.sendSysEx(data, length);
}

void ControlAPI::sendProgramChange(uint8_t channel, uint8_t program) {
    midi_.sendProgramChange(channel, program);
}

void ControlAPI::sendPitchBend(uint8_t channel, int16_t value) {
    midi_.sendPitchBend(channel, value);
}

// ═══════════════════════════════════════════════════
// MIDI Input
// ═══════════════════════════════════════════════════

void ControlAPI::onCC(hal::IMidiTransport::CCCallback cb) {
    midi_.setOnCC(std::move(cb));
}

void ControlAPI::onNoteOn(hal::IMidiTransport::NoteCallback cb) {
    midi_.setOnNoteOn(std::move(cb));
}

void ControlAPI::onNoteOff(hal::IMidiTransport::NoteCallback cb) {
    midi_.setOnNoteOff(std::move(cb));
}

void ControlAPI::onSysEx(hal::IMidiTransport::SysExCallback cb) {
    midi_.setOnSysEx(std::move(cb));
}

}  // namespace oc::api
