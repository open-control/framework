#pragma once

#include <oc/api/ButtonAPI.hpp>
#include <oc/api/ButtonProxy.hpp>
#include <oc/api/EncoderAPI.hpp>
#include <oc/api/EncoderProxy.hpp>
#include <oc/api/MidiAPI.hpp>
#include <oc/context/APIs.hpp>
#include <oc/core/input/ButtonBuilder.hpp>
#include <oc/core/input/EncoderBuilder.hpp>

namespace oc::context {

/**
 * @brief Base interface for application contexts
 *
 * A context represents a distinct application mode (e.g., Standalone, DAW integration).
 * Only ONE context is active at a time. Switching destroys the old and creates the new.
 *
 * Contexts declare their API requirements via static REQUIRES member:
 * @code
 * class MyContext : public IContext {
 *     static constexpr Requirements REQUIRES {
 *         .button = true,
 *         .encoder = true,
 *         .midi = true
 *     };
 *     // ...
 * };
 * @endcode
 *
 * Access APIs via protected accessors:
 * @code
 * bool initialize() override {
 *     // Bindings - "on" prefix for events
 *     onButton(BTN_1).press().then([this]{ midi().sendCC(1,20,127); });
 *     onEncoder(ENC_1).turn().then([this](float v){ updateValue(v); });
 *
 *     // State by ID - via proxy
 *     button(BTN_1).setLatch(true);
 *     encoder(ENC_1).setMode(EncoderMode::RELATIVE);
 *
 *     // Global operations - plural form
 *     buttons().clearScope(someScope);
 *     return true;
 * }
 * @endcode
 */
class IContext {
public:
    virtual ~IContext() = default;

    /**
     * @brief Called by framework before initialize()
     * @param apis Reference to APIs struct with available API pointers
     */
    void setAPIs(const APIs& apis) { apis_ = &apis; }

    // ═══════════════════════════════════════════════════
    // Lifecycle (must implement)
    // ═══════════════════════════════════════════════════

    /**
     * @brief Initialize the context
     *
     * Called after setAPIs(). Set up bindings, initialize state.
     * @return true if initialization succeeded
     */
    virtual bool initialize() = 0;

    /// Called every frame while context is active
    virtual void update() = 0;

    /// Called when context is being destroyed
    virtual void cleanup() = 0;

    /// Human-readable display name (for logging/debug only)
    virtual const char* getName() const = 0;

    // ═══════════════════════════════════════════════════
    // Connection state (optional override for DAW contexts)
    // ═══════════════════════════════════════════════════

    /// Check if external connection is active (default: always true)
    virtual bool isConnected() const { return true; }

    /// Called when connection is established
    virtual void onConnected() {}

    /// Called when connection is lost (before switchToDefault)
    virtual void onDisconnected() {}

protected:
    // ═══════════════════════════════════════════════════
    // Bindings - "on" prefix = event callbacks
    // ═══════════════════════════════════════════════════

    /**
     * @brief Start building a button binding
     * @param id Button to bind
     * @return ButtonBuilder for fluent configuration
     */
    [[nodiscard]] core::input::ButtonBuilder onButton(hal::ButtonID id) {
        return apis_->button->button(id);
    }

    /**
     * @brief Start building an encoder binding
     * @param id Encoder to bind
     * @return EncoderBuilder for fluent configuration
     */
    [[nodiscard]] core::input::EncoderBuilder onEncoder(hal::EncoderID id) {
        return apis_->encoder->encoder(id);
    }

    // ═══════════════════════════════════════════════════
    // State by ID - returns lightweight proxy
    // ═══════════════════════════════════════════════════

    /**
     * @brief Get proxy for button state access
     * @param id Button ID
     * @return ButtonProxy for state operations
     */
    api::ButtonProxy button(hal::ButtonID id) {
        return api::ButtonProxy(*apis_->button, id);
    }

    /**
     * @brief Get proxy for encoder state access
     * @param id Encoder ID
     * @return EncoderProxy for state operations
     */
    api::EncoderProxy encoder(hal::EncoderID id) {
        return api::EncoderProxy(*apis_->encoder, id);
    }

    // ═══════════════════════════════════════════════════
    // Global APIs - plural form for all buttons/encoders
    // ═══════════════════════════════════════════════════

    /// Access to ButtonAPI for global operations (clearBindings, clearScope)
    api::ButtonAPI& buttons() { return *apis_->button; }

    /// Access to EncoderAPI for global operations
    api::EncoderAPI& encoders() { return *apis_->encoder; }

    /// Access to MidiAPI for MIDI I/O
    api::MidiAPI& midi() { return *apis_->midi; }

    /// Access to EventBus for custom event handling
    core::event::IEventBus& events() { return apis_->events; }

    // ═══════════════════════════════════════════════════
    // Availability checks
    // ═══════════════════════════════════════════════════

    /// Check if ButtonAPI is available
    bool hasButtons() const { return apis_->button != nullptr; }

    /// Check if EncoderAPI is available
    bool hasEncoders() const { return apis_->encoder != nullptr; }

    /// Check if MidiAPI is available
    bool hasMidi() const { return apis_->midi != nullptr; }

private:
    const APIs* apis_ = nullptr;
};

}  // namespace oc::context
