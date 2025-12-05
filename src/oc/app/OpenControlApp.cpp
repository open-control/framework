#include "OpenControlApp.hpp"

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
