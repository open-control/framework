#include "OpenControlApp.hpp"

#include <oc/core/event/Events.hpp>
#include <oc/log/Log.hpp>
#include <oc/state/NotificationQueue.hpp>

namespace oc::app {

OpenControlApp::~OpenControlApp() = default;

OpenControlApp::OpenControlApp(OpenControlApp&&) noexcept = default;
OpenControlApp& OpenControlApp::operator=(OpenControlApp&&) noexcept = default;

void OpenControlApp::begin() {
    // Helper: check result and halt on error
    auto check = [](const core::Result<void>& result, const char* component) {
        if (!result) {
            OC_LOG_ERROR("{} init failed: {}", component, core::errorCodeToString(result.error().code));
            while (true) {}  // Halt - no recovery in embedded
        }
    };

    // Initialize hardware components
    if (display_) check(display_->init(), "Display");
    if (midi_) check(midi_->init(), "MIDI");
    if (encoders_) check(encoders_->init(), "Encoders");
    if (buttons_) check(buttons_->init(), "Buttons");

    // Wire HAL callbacks to EventBus
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

    check(contexts_->begin(), "Context");
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

    // Flush deferred signal notifications (coalesced updates)
    state::NotificationQueue::instance().flush();
}

}  // namespace oc::app
