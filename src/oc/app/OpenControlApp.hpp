#pragma once

#include <memory>

#include <oc/api/ButtonAPI.hpp>
#include <oc/core/Result.hpp>
#include <oc/api/EncoderAPI.hpp>
#include <oc/api/MidiAPI.hpp>
#include <oc/context/APIs.hpp>
#include <oc/context/ContextManager.hpp>
#include <oc/core/event/EventBus.hpp>
#include <oc/core/input/InputBinding.hpp>
#include <oc/core/input/InputConfig.hpp>
#include <oc/hal/IButtonController.hpp>
#include <oc/hal/IDisplayDriver.hpp>
#include <oc/hal/IEncoderController.hpp>
#include <oc/hal/IMidiTransport.hpp>
#include <oc/hal/IMessageTransport.hpp>
#include <oc/hal/Types.hpp>

namespace oc::app {

class AppBuilder;

/**
 * @brief Main application class for Open Control framework
 *
 * Owns all framework components and provides the main application loop.
 * Created via AppBuilder for proper dependency injection.
 *
 * All hardware components are optional except timeProvider.
 * APIs are created conditionally based on available hardware:
 * - ButtonAPI: created if buttons controller is provided
 * - EncoderAPI: created if encoders controller is provided
 * - MidiAPI: created if MIDI transport is provided
 *
 * @code
 * OpenControlApp app = AppBuilder()
 *     .midi(std::make_unique<TeensyUsbMidi>())
 *     .encoders(std::make_unique<TeensyEncoderController<2>>(config))
 *     .buttons(std::make_unique<TeensyButtonController<2>>(config))
 *     .timeProvider(millis)
 *     .build();
 *
 * app.registerContext<MyContext>(ContextID::MAIN, "Main");
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
     *
     * Must be called once in setup() before update().
     * On error: logs the error and halts (embedded systems have no recovery).
     * Silent on success - only user-defined logs will appear.
     */
    void begin();

    /**
     * @brief Main application loop
     * Call every frame in loop()
     */
    void update();

    // ═══════════════════════════════════════════════════
    // API ACCESSORS
    // ═══════════════════════════════════════════════════

    /// Access to ButtonAPI (nullptr if no button controller)
    api::ButtonAPI* buttonAPI() { return button_api_.get(); }

    /// Access to EncoderAPI (nullptr if no encoder controller)
    api::EncoderAPI* encoderAPI() { return encoder_api_.get(); }

    /// Access to MidiAPI (nullptr if no MIDI transport)
    api::MidiAPI* midiAPI() { return midi_api_.get(); }

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
     * For input handling, prefer the fluent API in contexts:
     *   onButton(id).press().then(...)
     *   onEncoder(id).turn().then(...)
     *
     * @code
     * // Subscribe to context changes
     * app.eventBus().on(
     *     EventCategory::CONTEXT,
     *     SystemEvent::CONTEXT_ACTIVATED,
     *     [](const Event& e) {
     *         auto& ce = static_cast<const ContextActivatedEvent&>(e);
     *         Serial.printf("Context: %s\n", ce.contextName);
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
     * @param name Human-readable name for UI/debugging
     * @return true if registration succeeded
     *
     * @note Context requirements are validated at registration time.
     *       If T::REQUIRES specifies an API that wasn't provided to the builder,
     *       registration will fail.
     */
    template <typename T, typename ID>
    bool registerContext(ID id, const char* name) {
        return contexts_->registerContext<T>(id, name);
    }

    /**
     * @brief Register a context with a custom factory function
     * @tparam ID Enum type convertible to uint8_t
     * @param id Context identifier
     * @param name Human-readable name
     * @param factory Factory function that creates the context
     * @return true if registration succeeded
     */
    template <typename ID>
    bool registerContextWithFactory(ID id, const char* name,
                                    context::ContextManager::ContextFactory factory) {
        return contexts_->registerContextWithFactory(id, name, std::move(factory));
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

    // ═══════════════════════════════════════════════════
    // DEBUG / STATS
    // ═══════════════════════════════════════════════════

    /**
     * @brief Timing statistics for update() loop (requires OC_ENABLE_STATS=1)
     */
    struct TimingStats {
        uint32_t lastUpdateUs = 0;   ///< Last update() duration in microseconds
        uint32_t peakUpdateUs = 0;   ///< Peak update() duration ever seen
        uint32_t totalUpdates = 0;   ///< Total update() calls
        uint64_t totalUpdateUs = 0;  ///< Sum of all update() durations

        /// Average update duration in microseconds
        [[nodiscard]] uint32_t avgUpdateUs() const {
            return totalUpdates > 0 ? static_cast<uint32_t>(totalUpdateUs / totalUpdates) : 0;
        }
    };

    /**
     * @brief Log framework statistics (requires OC_ENABLE_STATS=1)
     *
     * Outputs EventBus, NotificationQueue, and timing stats via OC_LOG_INFO.
     * Call this periodically or on-demand for debugging.
     *
     * @code
     * // In your debug context or serial command handler
     * app.dumpStats();
     * @endcode
     */
    void dumpStats() const;

    /**
     * @brief Reset all framework statistics
     */
    void resetStats();

    /**
     * @brief Get timing statistics for update() loop
     */
    [[nodiscard]] const TimingStats& timingStats() const { return timing_stats_; }

    /**
     * @brief Estimate total framework memory usage in bytes
     *
     * Includes: EventBus, NotificationQueue, InputBinding, ContextManager
     * Does NOT include: HAL drivers, user contexts, Signal storage
     */
    [[nodiscard]] size_t estimateMemoryUsage() const;

private:
    OpenControlApp() = default;

    // ═══════════════════════════════════════════════════════════════════════
    // MEMBER DECLARATION ORDER IS CRITICAL FOR SAFE DESTRUCTION
    // C++ destroys members in REVERSE declaration order.
    //
    // Required destruction order:
    // 1. contexts_      - cleanup() calls clearBindings() on input_binding_
    // 2. input_binding_ - destructor calls event_bus_.off()
    // 3. event_bus_     - destroyed LAST (other components may unsubscribe)
    //
    // DO NOT REORDER these members without understanding the implications!
    // ═══════════════════════════════════════════════════════════════════════

    // Hardware (owned via unique_ptr)
    std::unique_ptr<hal::IDisplayDriver> display_;
    std::unique_ptr<hal::IMidiTransport> midi_;
    std::unique_ptr<hal::IMessageTransport> frames_;
    std::unique_ptr<hal::IEncoderController> encoders_;
    std::unique_ptr<hal::IButtonController> buttons_;

    // Core services - event_bus_ MUST be declared before input_binding_
    core::event::EventBus event_bus_;
    std::unique_ptr<core::input::InputBinding> input_binding_;

    // APIs (owned, conditionally created)
    std::unique_ptr<api::ButtonAPI> button_api_;
    std::unique_ptr<api::EncoderAPI> encoder_api_;
    std::unique_ptr<api::MidiAPI> midi_api_;
    std::unique_ptr<context::APIs> apis_;

    // Context management - MUST be declared AFTER input_binding_ and event_bus_
    std::unique_ptr<context::ContextManager> contexts_;

    // Configuration (trivial destruction, order doesn't matter)
    core::InputConfig input_config_;
    hal::TimeProvider time_provider_ = nullptr;

    // Statistics (only active when OC_ENABLE_STATS=1)
    mutable TimingStats timing_stats_{};
};

}  // namespace oc::app
