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
 * - midi: MIDI transport driver
 * - encoders: Encoder controller driver
 * - buttons: Button controller driver
 * - timeProvider: Platform-specific milliseconds function (e.g., millis on Arduino)
 *
 * Optional components:
 * - display: Display driver (nullptr if no display)
 * - inputConfig: Gesture timing configuration (defaults provided)
 *
 * @code
 * auto app = AppBuilder()
 *     .timeProvider(millis)  // Required: platform time source
 *     .midi(std::make_unique<TeensyUsbMidi>())
 *     .encoders(std::make_unique<EncoderController<2>>(encoders))
 *     .buttons(std::make_unique<ButtonController<4>>(buttons))
 *     .display(std::make_unique<Ili9341Driver>(displayConfig))  // Optional
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
     * @note Asserts if required components (midi, encoders, buttons, timeProvider) are missing
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
