#pragma once

#include <oc/core/event/IEventBus.hpp>

namespace oc::api {
class ButtonAPI;
class EncoderAPI;
class MidiAPI;
}  // namespace oc::api

namespace oc::context {

class IContextSwitcher;

/**
 * @brief Service container providing API access to contexts
 *
 * This struct aggregates all the APIs and services that contexts may need.
 * It is passed to each context via setAPIs() before initialize() is called.
 *
 * ## Ownership
 *
 * APIs does NOT own any of these pointers - it holds non-owning references
 * to services managed by OpenControlApp. All pointers remain valid for the
 * lifetime of the application.
 *
 * ## Optional APIs
 *
 * The button, encoder, midi, and contexts pointers may be nullptr if the
 * corresponding subsystem is not configured. Contexts should declare their
 * requirements via the static REQUIRES member to ensure availability at
 * registration time.
 *
 * ## Usage in Context
 *
 * @code
 * class MyContext : public IContext {
 *     bool initialize() override {
 *         // APIs are available via apis() after setAPIs() is called
 *         button().onPress(ButtonID::PLAY, [this] { play(); });
 *         encoder().onTurn(EncoderID::VOLUME, [this](int v) { setVolume(v); });
 *         return true;
 *     }
 * };
 * @endcode
 *
 * @see IContext::setAPIs()
 * @see Requirements
 */
struct APIs {
    /**
     * @brief Button input API (optional)
     *
     * Provides button binding registration and state queries.
     * May be nullptr if no buttons are configured.
     */
    api::ButtonAPI* button = nullptr;

    /**
     * @brief Encoder input API (optional)
     *
     * Provides encoder binding registration and value management.
     * May be nullptr if no encoders are configured.
     */
    api::EncoderAPI* encoder = nullptr;

    /**
     * @brief MIDI output API (optional)
     *
     * Provides MIDI message sending (notes, CC, sysex, etc.).
     * May be nullptr if MIDI is not configured.
     */
    api::MidiAPI* midi = nullptr;

    /**
     * @brief Context switching interface (optional)
     *
     * Provides deferred context switching and context enumeration.
     * Set automatically by AppBuilder after ContextManager is created.
     * May be nullptr during early initialization.
     *
     * @see IContextSwitcher
     */
    IContextSwitcher* contexts = nullptr;

    /**
     * @brief Event bus for pub/sub messaging (required)
     *
     * Central event bus for decoupled communication between components.
     * Always valid - passed at construction.
     */
    core::event::IEventBus& events;

    /**
     * @brief Construct APIs with required event bus reference
     * @param e Reference to the application event bus
     */
    explicit APIs(core::event::IEventBus& e) : events(e) {}
};

}  // namespace oc::context
