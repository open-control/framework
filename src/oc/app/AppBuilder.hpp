#pragma once

#include <memory>

#include <oc/core/input/InputConfig.hpp>
#include <oc/hal/IButtonController.hpp>
#include <oc/hal/IDisplayDriver.hpp>
#include <oc/hal/IEncoderController.hpp>
#include <oc/hal/IMidiTransport.hpp>
#include <oc/hal/Types.hpp>

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

    AppBuilder& display(std::unique_ptr<hal::IDisplayDriver> driver);
    AppBuilder& midi(std::unique_ptr<hal::IMidiTransport> transport);
    AppBuilder& encoders(std::unique_ptr<hal::IEncoderController> controller);
    AppBuilder& buttons(std::unique_ptr<hal::IButtonController> controller);
    AppBuilder& inputConfig(const core::InputConfig& config);
    AppBuilder& timeProvider(hal::TimeProvider provider);

    /**
     * @brief Build the OpenControlApp with configured components
     * @return Configured OpenControlApp instance
     * @note Asserts if timeProvider is missing (only required component)
     */
    OpenControlApp build();

private:
    std::unique_ptr<hal::IDisplayDriver> display_;
    std::unique_ptr<hal::IMidiTransport> midi_;
    std::unique_ptr<hal::IEncoderController> encoders_;
    std::unique_ptr<hal::IButtonController> buttons_;
    core::InputConfig input_config_;
    hal::TimeProvider time_provider_ = nullptr;
};

}  // namespace oc::app
