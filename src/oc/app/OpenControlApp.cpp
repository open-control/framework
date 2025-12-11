#include "OpenControlApp.hpp"

#include <oc/core/event/Events.hpp>

namespace oc::app {

OpenControlApp::~OpenControlApp() = default;

OpenControlApp::OpenControlApp(OpenControlApp&&) noexcept = default;
OpenControlApp& OpenControlApp::operator=(OpenControlApp&&) noexcept = default;

core::Result<void> OpenControlApp::begin() {
    // Display is optional
    if (display_) {
        auto result = display_->init();
        if (!result) return result;
    }

    // Required components
    if (midi_) {
        auto result = midi_->init();
        if (!result) return result;
    }
    if (encoders_) {
        auto result = encoders_->init();
        if (!result) return result;
    }
    if (buttons_) {
        auto result = buttons_->init();
        if (!result) return result;
    }

    // ═══════════════════════════════════════════════════
    // Wire HAL callbacks to EventBus
    // ═══════════════════════════════════════════════════

    if (buttons_) {
        buttons_->setCallback([this](hal::ButtonID id, hal::ButtonEvent evt) {
            if (evt == hal::ButtonEvent::PRESSED) {
                event_bus_.emit(core::event::ButtonPressEvent(id, true));
            } else {
                event_bus_.emit(core::event::ButtonReleaseEvent(id));
            }
        });
    }

    if (encoders_) {
        encoders_->setCallback([this](hal::EncoderID id, float value) {
            event_bus_.emit(core::event::EncoderChangedEvent(id, value));
        });
    }

    if (midi_) {
        midi_->setOnCC([this](uint8_t ch, uint8_t cc, uint8_t val) {
            event_bus_.emit(core::event::MidiCCEvent(ch, cc, val));
        });
        midi_->setOnNoteOn([this](uint8_t ch, uint8_t note, uint8_t vel) {
            event_bus_.emit(core::event::MidiNoteOnEvent(ch, note, vel));
        });
        midi_->setOnNoteOff([this](uint8_t ch, uint8_t note, uint8_t vel) {
            event_bus_.emit(core::event::MidiNoteOffEvent(ch, note, vel));
        });
        midi_->setOnSysEx([this](const uint8_t* data, size_t len) {
            event_bus_.emit(core::event::SysExEvent(data, static_cast<uint16_t>(len)));
        });
    }

    return contexts_->begin();
}

void OpenControlApp::update() {
    uint32_t now = time_provider_();

    if (midi_) {
        midi_->update();
    }
    if (encoders_) {
        encoders_->update();
    }
    if (buttons_) {
        buttons_->update(now);
    }
    if (input_binding_) {
        input_binding_->processTick();
    }

    contexts_->update();
}

}  // namespace oc::app
