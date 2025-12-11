#pragma once

#include <cassert>

#include <oc/api/ButtonAPI.hpp>
#include <oc/api/ButtonProxy.hpp>
#include <oc/api/EncoderAPI.hpp>
#include <oc/api/EncoderProxy.hpp>
#include <oc/api/MidiAPI.hpp>
#include <oc/context/APIs.hpp>
#include <oc/context/IContextSwitcher.hpp>
#include <oc/core/input/ButtonBuilder.hpp>
#include <oc/core/input/EncoderBuilder.hpp>

namespace oc::context {

/**
 * @brief Base class for application contexts (screens/modes)
 *
 * A context represents a distinct mode of operation in the application,
 * such as a main menu, settings screen, or DAW control surface. Each context
 * manages its own input bindings, UI state, and behavior.
 *
 * ## Lifecycle
 *
 * Contexts follow a strict lifecycle managed by ContextManager:
 *
 * 1. **Construction**: Factory creates instance (no APIs available yet)
 * 2. **setAPIs()**: Framework injects API references
 * 3. **initialize()**: Context sets up bindings and initial state
 * 4. **onConnected()**: Called after successful initialization
 * 5. **update()**: Called every frame while active
 * 6. **onDisconnected()**: Called before cleanup (for DAW contexts)
 * 7. **cleanup()**: Context releases resources
 * 8. **Destruction**: Instance is destroyed
 *
 * ## Implementation Example
 *
 * @code
 * class MainContext : public IContext {
 * public:
 *     static constexpr Requirements REQUIRES{
 *         .button = true,
 *         .encoder = true,
 *         .midi = false
 *     };
 *
 *     bool initialize() override {
 *         onButton(ButtonID::PLAY).onPress([this] { play(); });
 *         onEncoder(EncoderID::VOLUME).onTurn([this](int v) { setVolume(v); });
 *         return true;
 *     }
 *
 *     void update() override {
 *         // Called every frame
 *     }
 *
 *     void cleanup() override {
 *         // Bindings are auto-cleared by ContextManager
 *     }
 *
 *     const char* getName() const override { return "Main"; }
 * };
 * @endcode
 *
 * ## Context Switching
 *
 * Contexts can switch to other contexts using the protected switchTo() method.
 * Switching is **deferred** - the actual switch happens after update() returns,
 * ensuring safe lifecycle management.
 *
 * @code
 * void onSettingsPressed() {
 *     switchTo(ContextID::SETTINGS);  // Deferred switch
 * }
 * @endcode
 *
 * @see ContextManager
 * @see Requirements
 * @see IContextSwitcher
 */
class IContext {
public:
    virtual ~IContext() = default;

    /**
     * @brief Inject API references (called by ContextManager before initialize)
     * @param apis Reference to the APIs container
     * @note Do not call this directly - managed by ContextManager
     */
    void setAPIs(const APIs& apis) { apis_ = &apis; }

    // ─────────────────────────────────────────────────────────────────────
    // Lifecycle (must implement)
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Initialize the context after APIs are available
     *
     * Set up input bindings, subscribe to events, initialize UI, etc.
     * Called after setAPIs() and before onConnected().
     *
     * @return true if initialization succeeded, false to trigger fallback
     */
    virtual bool initialize() = 0;

    /**
     * @brief Update the context (called every frame while active)
     *
     * Handle per-frame logic, animations, polling, etc.
     * Input events are delivered via bindings, not here.
     */
    virtual void update() = 0;

    /**
     * @brief Clean up resources before destruction
     *
     * Release any resources, unsubscribe from events, etc.
     * Input bindings are automatically cleared by ContextManager.
     */
    virtual void cleanup() = 0;

    /**
     * @brief Get the human-readable name of this context
     * @return Static string identifying this context (for debugging/UI)
     */
    virtual const char* getName() const = 0;

    // ─────────────────────────────────────────────────────────────────────
    // Connection State (optional override for DAW contexts)
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Check if the context is still connected
     *
     * For DAW contexts, this indicates whether the DAW connection is alive.
     * When this returns false, ContextManager will call onDisconnected()
     * and may switch to the default context.
     *
     * @return true if connected (default), false if disconnected
     */
    virtual bool isConnected() const { return true; }

    /**
     * @brief Called when the context becomes active
     *
     * Override to perform actions when this context is activated,
     * such as sending initial state to a DAW or updating display.
     */
    virtual void onConnected() {}

    /**
     * @brief Called when the context is about to be deactivated
     *
     * Override to perform cleanup when losing focus, such as
     * sending "goodbye" messages to a DAW or saving state.
     */
    virtual void onDisconnected() {}

protected:
    // ─────────────────────────────────────────────────────────────────────
    // Input Binding Builders (fluent API)
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Start building a button binding
     *
     * Returns a fluent builder for configuring button actions.
     *
     * @tparam ID Enum class or integral type convertible to hal::ButtonID
     * @param id Button identifier
     * @return ButtonBuilder for chaining configuration
     *
     * @code
     * onButton(ButtonID::PLAY)
     *     .onPress([this] { transport.play(); })
     *     .onLongPress([this] { transport.stop(); }, 500);
     * @endcode
     */
    template <typename ID>
    [[nodiscard]] core::input::ButtonBuilder onButton(ID id) {
        assert(apis_ && "setAPIs() not called - context not properly initialized");
        assert(apis_->button && "ButtonAPI not available");
        return apis_->button->button(static_cast<hal::ButtonID>(id));
    }

    /**
     * @brief Start building an encoder binding
     *
     * Returns a fluent builder for configuring encoder actions.
     *
     * @tparam ID Enum class or integral type convertible to hal::EncoderID
     * @param id Encoder identifier
     * @return EncoderBuilder for chaining configuration
     *
     * @code
     * onEncoder(EncoderID::VOLUME)
     *     .withRange(0, 127)
     *     .onTurn([this](int value) { setVolume(value); });
     * @endcode
     */
    template <typename ID>
    [[nodiscard]] core::input::EncoderBuilder onEncoder(ID id) {
        assert(apis_ && "setAPIs() not called - context not properly initialized");
        assert(apis_->encoder && "EncoderAPI not available");
        return apis_->encoder->encoder(static_cast<hal::EncoderID>(id));
    }

    // ─────────────────────────────────────────────────────────────────────
    // State Proxies (for querying input state)
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Get a proxy for querying button state
     * @tparam ID Enum class or integral type convertible to hal::ButtonID
     * @param id Button identifier
     * @return ButtonProxy for state queries (isPressed, isLatched, etc.)
     */
    template <typename ID>
    api::ButtonProxy button(ID id) {
        assert(apis_ && "setAPIs() not called - context not properly initialized");
        assert(apis_->button && "ButtonAPI not available");
        return api::ButtonProxy(*apis_->button, static_cast<hal::ButtonID>(id));
    }

    /**
     * @brief Get a proxy for querying encoder state
     * @tparam ID Enum class or integral type convertible to hal::EncoderID
     * @param id Encoder identifier
     * @return EncoderProxy for state queries (value, position, etc.)
     */
    template <typename ID>
    api::EncoderProxy encoder(ID id) {
        assert(apis_ && "setAPIs() not called - context not properly initialized");
        assert(apis_->encoder && "EncoderAPI not available");
        return api::EncoderProxy(*apis_->encoder, static_cast<hal::EncoderID>(id));
    }

    // ─────────────────────────────────────────────────────────────────────
    // Global API Access
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Access the ButtonAPI for advanced operations
     * @return Reference to ButtonAPI
     */
    api::ButtonAPI& buttons() {
        assert(apis_ && "setAPIs() not called - context not properly initialized");
        assert(apis_->button && "ButtonAPI not available");
        return *apis_->button;
    }

    /**
     * @brief Access the EncoderAPI for advanced operations
     * @return Reference to EncoderAPI
     */
    api::EncoderAPI& encoders() {
        assert(apis_ && "setAPIs() not called - context not properly initialized");
        assert(apis_->encoder && "EncoderAPI not available");
        return *apis_->encoder;
    }

    /**
     * @brief Access the MidiAPI for sending MIDI messages
     * @return Reference to MidiAPI
     */
    api::MidiAPI& midi() {
        assert(apis_ && "setAPIs() not called - context not properly initialized");
        assert(apis_->midi && "MidiAPI not available");
        return *apis_->midi;
    }

    /**
     * @brief Access the event bus for pub/sub messaging
     * @return Reference to IEventBus
     */
    core::event::IEventBus& events() {
        assert(apis_ && "setAPIs() not called - context not properly initialized");
        return apis_->events;
    }

    // ─────────────────────────────────────────────────────────────────────
    // Context Switching (deferred - safe to call from update/handlers)
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Request a switch to another context
     *
     * The switch is **deferred** - it will occur after update() returns.
     * This ensures safe lifecycle management (no use-after-free).
     *
     * @tparam ID Enum class or integral type for context IDs
     * @param id Target context identifier
     *
     * @code
     * switchTo(ContextID::SETTINGS);
     * // Still executing in current context here
     * // Switch happens after update() returns
     * @endcode
     */
    template <typename ID>
    void switchTo(ID id) {
        assert(apis_ && "setAPIs() not called - context not properly initialized");
        assert(apis_->contexts && "ContextSwitcher not available");
        apis_->contexts->switchTo(id);
    }

    /**
     * @brief Request a switch to the default context
     *
     * Deferred switch to the context marked as default.
     * Useful for "back to home" or "escape" actions.
     */
    void switchToDefault() {
        assert(apis_ && "setAPIs() not called - context not properly initialized");
        assert(apis_->contexts && "ContextSwitcher not available");
        apis_->contexts->switchToDefault();
    }

    // ─────────────────────────────────────────────────────────────────────
    // Context Queries
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Check if a context is registered
     * @tparam ID Enum class or integral type
     * @param id Context identifier to check
     * @return true if context exists
     */
    template <typename ID>
    bool hasContext(ID id) const {
        assert(apis_ && "setAPIs() not called - context not properly initialized");
        assert(apis_->contexts && "ContextSwitcher not available");
        return apis_->contexts->hasContext(id);
    }

    /**
     * @brief Get the name of a registered context
     * @param id Raw context ID
     * @return Context name, or nullptr if not registered
     */
    const char* contextName(uint8_t id) const {
        assert(apis_ && "setAPIs() not called - context not properly initialized");
        assert(apis_->contexts && "ContextSwitcher not available");
        return apis_->contexts->contextName(id);
    }

    /**
     * @brief Get the ID of the currently active context
     * @return Active context ID (should be this context's ID)
     */
    uint8_t activeContextId() const {
        assert(apis_ && "setAPIs() not called - context not properly initialized");
        assert(apis_->contexts && "ContextSwitcher not available");
        return apis_->contexts->activeId();
    }

    /**
     * @brief Get the ID of the default context
     * @return Default context ID
     */
    uint8_t defaultContextId() const {
        assert(apis_ && "setAPIs() not called - context not properly initialized");
        assert(apis_->contexts && "ContextSwitcher not available");
        return apis_->contexts->defaultId();
    }

    /**
     * @brief Get the number of registered contexts
     * @return Count of registered contexts
     */
    size_t contextCount() const {
        assert(apis_ && "setAPIs() not called - context not properly initialized");
        assert(apis_->contexts && "ContextSwitcher not available");
        return apis_->contexts->contextCount();
    }

    /**
     * @brief Iterate over all registered contexts
     *
     * Useful for building context selection menus.
     *
     * @tparam Fn Callable accepting const ContextInfo&
     * @param fn Callback invoked for each context
     *
     * @code
     * forEachContext([this](const ContextInfo& info) {
     *     menu.addItem(info.name, [id = info.id, this] {
     *         switchTo(id);
     *     });
     * });
     * @endcode
     */
    template <typename Fn>
    void forEachContext(Fn&& fn) const {
        assert(apis_ && "setAPIs() not called - context not properly initialized");
        assert(apis_->contexts && "ContextSwitcher not available");
        apis_->contexts->forEachContext(std::forward<Fn>(fn));
    }

    // ─────────────────────────────────────────────────────────────────────
    // Availability Checks
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Check if ButtonAPI is available
     * @return true if buttons can be used
     */
    bool hasButtons() const { return apis_->button != nullptr; }

    /**
     * @brief Check if EncoderAPI is available
     * @return true if encoders can be used
     */
    bool hasEncoders() const { return apis_->encoder != nullptr; }

    /**
     * @brief Check if MidiAPI is available
     * @return true if MIDI can be used
     */
    bool hasMidi() const { return apis_->midi != nullptr; }

private:
    const APIs* apis_ = nullptr;  ///< Injected API references (set by ContextManager)
};

}  // namespace oc::context
