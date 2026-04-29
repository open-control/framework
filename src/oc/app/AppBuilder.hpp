#pragma once

#include <memory>

#include <oc/core/input/InputConfig.hpp>
#include <oc/core/input/InputBindingTrace.hpp>
#include <oc/interface/IButton.hpp>
#include <oc/interface/IDisplay.hpp>
#include <oc/interface/IEncoder.hpp>
#include <oc/interface/IMidi.hpp>
#include <oc/interface/ITransport.hpp>
#include <oc/type/Ids.hpp>
#include <oc/type/Callbacks.hpp>

namespace oc::app {

class OpenControlApp;

/**
 * @brief Fluent builder for OpenControlApp configuration
 *
 * Allows configuring all hardware drivers and settings before
 * constructing the application.
 *
 * Required components:
 * - timeProvider: Platform-specific milliseconds function (e.g., millis on Arduino)
 *
 * Optional components (APIs created conditionally):
 * - buttons: Button controller -> creates ButtonAPI
 * - encoders: Encoder controller -> creates EncoderAPI
 * - midi: MIDI transport -> creates MidiAPI
 * - display: Display driver (for UI frameworks)
 * - inputConfig: Gesture timing configuration (defaults provided)
 *
 * @code
 * // Full configuration
 * auto app = AppBuilder()
 *     .timeProvider(millis)  // Required
 *     .buttons(std::make_unique<ButtonController<4>>(buttons))
 *     .encoders(std::make_unique<EncoderController<2>>(encoders))
 *     .midi(std::make_unique<TeensyUsbMidi>())
 *     .display(std::make_unique<Ili9341Driver>(displayConfig))
 *     .build();
 *
 * // Minimal configuration (buttons only)
 * auto app = AppBuilder()
 *     .timeProvider(millis)
 *     .buttons(std::make_unique<ButtonController<4>>(buttons))
 *     .build();
 * @endcode
 */
class AppBuilder {
public:
    AppBuilder() = default;

    // Non-copyable (contains unique_ptrs)
    AppBuilder(const AppBuilder&) = delete;
    AppBuilder& operator=(const AppBuilder&) = delete;

    // Moveable
    AppBuilder(AppBuilder&&) = default;
    AppBuilder& operator=(AppBuilder&&) = default;

    AppBuilder& display(std::unique_ptr<interface::IDisplay> driver);
    AppBuilder& midi(std::unique_ptr<interface::IMidi> transport);
    AppBuilder& frames(std::unique_ptr<interface::ITransport> transport);
    AppBuilder& encoders(std::unique_ptr<interface::IEncoder> controller);
    AppBuilder& buttons(std::unique_ptr<interface::IButton> controller);
    AppBuilder& inputConfig(const core::input::InputConfig& config);
    AppBuilder& inputTrace(core::input::InputBindingTraceCallback callback);
    AppBuilder& timeProvider(oc::type::TimeProvider provider);

    /**
     * @brief Build the OpenControlApp with configured components
     * @return Configured OpenControlApp instance
     * @note Asserts if timeProvider is missing (only required component)
     */
    OpenControlApp build();

private:
    std::unique_ptr<interface::IDisplay> display_;
    std::unique_ptr<interface::IMidi> midi_;
    std::unique_ptr<interface::ITransport> frames_;
    std::unique_ptr<interface::IEncoder> encoders_;
    std::unique_ptr<interface::IButton> buttons_;
    core::input::InputConfig input_config_;
    core::input::InputBindingTraceCallback input_trace_callback_;
    oc::type::TimeProvider time_provider_ = nullptr;
};

}  // namespace oc::app
