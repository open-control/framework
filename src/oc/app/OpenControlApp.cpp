#include "OpenControlApp.hpp"

namespace oc::app {

OpenControlApp::~OpenControlApp() = default;

OpenControlApp::OpenControlApp(OpenControlApp&&) noexcept = default;
OpenControlApp& OpenControlApp::operator=(OpenControlApp&&) noexcept = default;

void OpenControlApp::begin() {
    if (display_) {
        display_->init();
    }
    if (midi_) {
        midi_->init();
    }
    if (encoders_) {
        encoders_->init();
    }
    if (buttons_) {
        buttons_->init();
    }

    contexts_->switchToDefault();
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
