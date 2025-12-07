#pragma once

#include <memory>

#include <oc/api/ControlAPI.hpp>
#include <oc/context/ContextManager.hpp>
#include <oc/core/event/EventBus.hpp>
#include <oc/core/input/InputBinding.hpp>
#include <oc/core/input/InputConfig.hpp>
#include <oc/hal/IButtonController.hpp>
#include <oc/hal/IDisplayDriver.hpp>
#include <oc/hal/IEncoderController.hpp>
#include <oc/hal/IMidiTransport.hpp>
#include <oc/hal/Types.hpp>

namespace oc::app {

class AppBuilder;

/**
 * @brief Main application class for Open Control framework
 *
 * Owns all framework components and provides the main application loop.
 * Created via AppBuilder for proper dependency injection.
 *
 * @code
 * OpenControlApp app = AppBuilder()
 *     .midi(std::make_unique<TeensyUsbMidi>())
 *     .encoders(std::make_unique<TeensyEncoderController<2>>(config))
 *     .buttons(std::make_unique<TeensyButtonController<2>>(config))
 *     .build();
 *
 * app.registerContext<MyContext>("my-context");
 * app.begin();
 *
 * void loop() { app.update(); }
 * @endcode
 */
class OpenControlApp {
public:
    friend class AppBuilder;

    ~OpenControlApp();

    OpenControlApp(const OpenControlApp&) = delete;
    OpenControlApp& operator=(const OpenControlApp&) = delete;
    OpenControlApp(OpenControlApp&&) noexcept;
    OpenControlApp& operator=(OpenControlApp&&) noexcept;

    /**
     * @brief Initialize all hardware and services
     * Must be called once in setup() before update()
     * @return true if all hardware initialized successfully
     */
    bool begin();

    /**
     * @brief Main application loop
     * Call every frame in loop()
     */
    void update();

    // ═══════════════════════════════════════════════════
    // ACCESSORS
    // ═══════════════════════════════════════════════════

    /// Access to ControlAPI for input bindings, MIDI, and encoder control
    api::ControlAPI& api() { return *api_; }

    /// Access to ContextManager for context switching
    context::ContextManager& contexts() { return *contexts_; }

    /**
     * @brief Access to the internal event bus
     *
     * Use cases:
     * - Subscribe to system events (ContextActivated, BootComplete, etc.)
     * - Custom event logging/monitoring
     * - Advanced: emit custom events for your own modules
     *
     * For input handling, prefer ControlAPI methods (onPressed, onTurned, etc.)
     * which provide gesture detection and scoped bindings.
     *
     * @code
     * // Subscribe to context changes
     * app.eventBus().on(
     *     EventCategory::CONTEXT,
     *     SystemEvent::CONTEXT_ACTIVATED,
     *     [](const Event& e) {
     *         auto& ce = static_cast<const ContextActivatedEvent&>(e);
     *         Serial.printf("Context: %s\n", ce.contextId);
     *     }
     * );
     * @endcode
     */
    core::event::IEventBus& eventBus() { return event_bus_; }

    // ═══════════════════════════════════════════════════
    // CONTEXT SHORTCUTS
    // ═══════════════════════════════════════════════════

    /**
     * @brief Register a context factory
     * @tparam T Context class implementing IContext
     * @tparam ID Enum type convertible to uint8_t (user-defined ContextID)
     * @param id Context identifier enum value
     * @return true if registration succeeded
     */
    template <typename T, typename ID>
    bool registerContext(ID id) {
        return contexts_->registerContext<T>(id);
    }

    /**
     * @brief Switch to a context by ID
     * @tparam ID Enum type convertible to uint8_t
     * @param id Context identifier
     * @return true if switch succeeded
     */
    template <typename ID>
    bool switchTo(ID id) {
        return contexts_->switchTo(id);
    }

    /**
     * @brief Set the default context
     * @tparam ID Enum type convertible to uint8_t
     * @param id Context identifier to use as default
     */
    template <typename ID>
    void setDefault(ID id) {
        contexts_->setDefault(id);
    }

    /// Switch to the default context
    void switchToDefault() { contexts_->switchToDefault(); }

private:
    OpenControlApp() = default;

    // Hardware (owned via unique_ptr)
    std::unique_ptr<hal::IDisplayDriver> display_;
    std::unique_ptr<hal::IMidiTransport> midi_;
    std::unique_ptr<hal::IEncoderController> encoders_;
    std::unique_ptr<hal::IButtonController> buttons_;

    // Core services (owned)
    core::event::EventBus event_bus_;
    std::unique_ptr<core::input::InputBinding> input_binding_;

    // API and context management
    std::unique_ptr<api::ControlAPI> api_;
    std::unique_ptr<context::ContextManager> contexts_;

    // Configuration
    core::InputConfig input_config_;
    hal::TimeProvider time_provider_ = nullptr;
};

}  // namespace oc::app
