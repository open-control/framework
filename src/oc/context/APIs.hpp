#pragma once

#include <oc/core/event/IEventBus.hpp>

namespace oc::api {
class ButtonAPI;
class EncoderAPI;
class MidiAPI;
}  // namespace oc::api

namespace oc::context {

/**
 * @brief Container for API pointers passed to contexts
 *
 * Holds optional pointers to ButtonAPI, EncoderAPI, MidiAPI,
 * and a required reference to the EventBus.
 *
 * Passed to IContext::setAPIs() by the framework before initialize().
 */
struct APIs {
    api::ButtonAPI* button = nullptr;
    api::EncoderAPI* encoder = nullptr;
    api::MidiAPI* midi = nullptr;
    core::event::IEventBus& events;

    explicit APIs(core::event::IEventBus& e) : events(e) {}
};

}  // namespace oc::context
