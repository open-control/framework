#pragma once

#include <memory>
#include <string>

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

    api::ControlAPI& api() { return *api_; }
    context::ContextManager& contexts() { return *contexts_; }
    core::event::IEventBus& eventBus() { return event_bus_; }

    // ═══════════════════════════════════════════════════
    // CONTEXT SHORTCUTS
    // ═══════════════════════════════════════════════════

    /**
     * @brief Register a context type
     * @tparam T Context class implementing IContext
     * @param id Unique identifier for this context
     * @return true if registration succeeded
     */
    template <typename T>
    bool registerContext(const std::string& id) {
        return contexts_->registerContext<T>(id);
    }

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
