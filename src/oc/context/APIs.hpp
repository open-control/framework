#pragma once

#include <oc/interface/IEventBus.hpp>
#include <oc/interface/ITransport.hpp>

namespace oc::api {
class ButtonAPI;
class EncoderAPI;
class MidiAPI;
}  // namespace oc::api

namespace oc::interface {
class IContextSwitcher;
}  // namespace oc::interface

namespace oc::context {

/**
 * @brief Service container providing API access to contexts
 *
 * This struct aggregates all the APIs and services that contexts may need.
 * It is injected into contexts (when supported) before init() is called.
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
 * Recommended pattern:
 * - Define strongly-typed IDs (enum class : uint16_t)
 * - Inherit from oc::context::ContextBase to get the fluent API
 *
 * @code
 * enum class Button : uint16_t { PLAY = 0 };
 * enum class Encoder : uint16_t { VOLUME = 0 };
 *
 * class MyContext : public oc::context::ContextBase {
 * public:
 *     oc::type::Result<void> init() override {
 *         onButton(Button::PLAY).press().then([this] { play(); });
 *         onEncoder(Encoder::VOLUME).turn().then([this](float v) { setVolume(v); });
 *         return oc::type::Result<void>::ok();
 *     }
 *     void update() override {}
 *     void cleanup() override {}
 *     const char* getName() const override { return "MyContext"; }
 * };
 * @endcode
 *
 * @see oc::context::ContextBase
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
     * @brief Frame transport API (optional)
     *
     * Provides frame-based communication for protocols.
     * May be nullptr if frame transport is not configured.
     */
    interface::ITransport* frames = nullptr;

    /**
     * @brief Context switching interface (optional)
     *
     * Provides deferred context switching and context enumeration.
     * Set automatically by AppBuilder after ContextManager is created.
     * May be nullptr during early initialization.
     *
     * @see interface::IContextSwitcher
     */
    interface::IContextSwitcher* contexts = nullptr;

    /**
     * @brief oc::type::Event bus for pub/sub messaging (required)
     *
     * Central event bus for decoupled communication between components.
     * Always valid - passed at construction.
     */
    interface::IEventBus& events;

    /**
     * @brief Construct APIs with required event bus reference
     * @param e Reference to the application event bus
     */
    explicit APIs(interface::IEventBus& e) : events(e) {}
};

}  // namespace oc::context
