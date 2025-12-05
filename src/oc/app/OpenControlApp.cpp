#include "OpenControlApp.hpp"

#include <oc/core/event/Events.hpp>

namespace oc::app {

OpenControlApp::~OpenControlApp() = default;

OpenControlApp::OpenControlApp(OpenControlApp&&) noexcept = default;
OpenControlApp& OpenControlApp::operator=(OpenControlApp&&) noexcept = default;

bool OpenControlApp::begin() {
    // Display is optional
    if (display_ && !display_->init()) {
        return false;
    }

    // Required components
    if (midi_ && !midi_->init()) {
        return false;
    }
    if (encoders_ && !encoders_->init()) {
        return false;
    }
    if (buttons_ && !buttons_->init()) {
        return false;
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

    contexts_->switchToDefault();
    return true;
}

void OpenControlApp::update() {
    if (midi_) {
        midi_->update();
    }
    if (encoders_) {
        encoders_->update();
    }
    if (buttons_) {
        buttons_->update();
    }
    if (input_binding_) {
        input_binding_->processTick(time_provider_());
    }

    contexts_->update();
}

}  // namespace oc::app
