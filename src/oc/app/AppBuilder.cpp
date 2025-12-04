#include "AppBuilder.hpp"

#include <cassert>

#include "OpenControlApp.hpp"

namespace oc::app {

AppBuilder& AppBuilder::display(std::unique_ptr<hal::IDisplayDriver> driver) {
    display_ = std::move(driver);
    return *this;
}

AppBuilder& AppBuilder::midi(std::unique_ptr<hal::IMidiTransport> transport) {
    midi_ = std::move(transport);
    return *this;
}

AppBuilder& AppBuilder::encoders(std::unique_ptr<hal::IEncoderController> controller) {
    encoders_ = std::move(controller);
    return *this;
}

AppBuilder& AppBuilder::buttons(std::unique_ptr<hal::IButtonController> controller) {
    buttons_ = std::move(controller);
    return *this;
}

AppBuilder& AppBuilder::inputConfig(const core::InputConfig& config) {
    input_config_ = config;
    return *this;
}

AppBuilder& AppBuilder::timeProvider(hal::TimeProvider provider) {
    time_provider_ = provider;
    return *this;
}

OpenControlApp AppBuilder::build() {
    assert(midi_ && "AppBuilder: MIDI transport is required");
    assert(encoders_ && "AppBuilder: Encoder controller is required");
    assert(buttons_ && "AppBuilder: Button controller is required");
    assert(time_provider_ && "AppBuilder: TimeProvider is required");

    OpenControlApp app;

    // Transfer hardware ownership
    app.display_ = std::move(display_);
    app.midi_ = std::move(midi_);
    app.encoders_ = std::move(encoders_);
    app.buttons_ = std::move(buttons_);
    app.input_config_ = input_config_;
    app.time_provider_ = time_provider_;

    // Create InputBinding with EventBus reference
    app.input_binding_ =
        std::make_unique<core::input::InputBinding>(app.event_bus_, app.input_config_);

    // Create ControlAPI with all dependencies
    app.api_ = std::make_unique<api::ControlAPI>(*app.input_binding_, app.event_bus_,
                                                  *app.midi_, *app.encoders_);

    // Create ContextManager with ControlAPI
    app.contexts_ = std::make_unique<context::ContextManager>(*app.api_);

    return app;
}

}  // namespace oc::app
