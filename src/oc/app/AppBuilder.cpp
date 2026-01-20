#include "AppBuilder.hpp"

#include <cassert>

#include "OpenControlApp.hpp"
#include <oc/log/Log.hpp>

namespace oc::app {

AppBuilder& AppBuilder::display(std::unique_ptr<interface::IDisplay> driver) {
    display_ = std::move(driver);
    return *this;
}

AppBuilder& AppBuilder::midi(std::unique_ptr<interface::IMidi> transport) {
    midi_ = std::move(transport);
    return *this;
}

AppBuilder& AppBuilder::frames(std::unique_ptr<interface::ITransport> transport) {
    frames_ = std::move(transport);
    return *this;
}

AppBuilder& AppBuilder::encoders(std::unique_ptr<interface::IEncoder> controller) {
    encoders_ = std::move(controller);
    return *this;
}

AppBuilder& AppBuilder::buttons(std::unique_ptr<interface::IButton> controller) {
    buttons_ = std::move(controller);
    return *this;
}

AppBuilder& AppBuilder::inputConfig(const core::InputConfig& config) {
    input_config_ = config;
    return *this;
}

AppBuilder& AppBuilder::timeProvider(TimeProvider provider) {
    time_provider_ = provider;
    // Note: Log API now uses HAL's getTimeMs() directly via weak symbols
    return *this;
}

OpenControlApp AppBuilder::build() {
    assert(time_provider_ && "AppBuilder: TimeProvider is required");

    OpenControlApp app;

    // Transfer hardware ownership
    app.display_ = std::move(display_);
    app.midi_ = std::move(midi_);
    app.frames_ = std::move(frames_);
    app.encoders_ = std::move(encoders_);
    app.buttons_ = std::move(buttons_);
    app.input_config_ = input_config_;
    app.time_provider_ = time_provider_;

    // Create InputBinding if buttons OR encoders available
    if (app.buttons_ || app.encoders_) {
        app.input_binding_ =
            std::make_unique<core::input::InputBinding>(app.event_bus_, app.time_provider_, app.input_config_);
    }

    // Create APIs conditionally based on available hardware
    if (app.buttons_ && app.input_binding_) {
        app.button_api_ = std::make_unique<api::ButtonAPI>(*app.input_binding_, *app.buttons_);
    }

    if (app.encoders_ && app.input_binding_) {
        app.encoder_api_ =
            std::make_unique<api::EncoderAPI>(*app.input_binding_, *app.encoders_);
    }

    if (app.midi_) {
        app.midi_api_ = std::make_unique<api::MidiAPI>(*app.midi_);
    }

    // Create APIs container
    app.apis_ = std::make_unique<context::APIs>(app.event_bus_);
    app.apis_->button = app.button_api_.get();
    app.apis_->encoder = app.encoder_api_.get();
    app.apis_->midi = app.midi_api_.get();
    app.apis_->frames = app.frames_.get();

    // Create ContextManager with APIs reference
    app.contexts_ = std::make_unique<context::ContextManager>(*app.apis_);

    // Wire ContextManager back to APIs (for IContext::requestSwitchTo)
    app.apis_->contexts = app.contexts_.get();

    return app;
}

}  // namespace oc::app
