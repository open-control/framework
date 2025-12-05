/**
 * @file main.cpp
 * @brief Open Control Framework - Minimal Teensy 4.1 Example
 *
 * Demonstrates:
 * - Hardware configuration with constexpr definitions
 * - AppBuilder for clean dependency injection
 * - ControlAPI for input binding (buttons and encoders)
 * - MIDI CC output
 *
 * Features shown:
 * - Button press → MIDI CC 127
 * - Button release → MIDI CC 0
 * - Button long press → Different action
 * - Encoder turn → MIDI CC (0-127 mapped from 0.0-1.0)
 */

#include <Arduino.h>

#include <optional>

// ═══════════════════════════════════════════════════════════════════
// Framework includes
// ═══════════════════════════════════════════════════════════════════

#include <oc/app/AppBuilder.hpp>
#include <oc/app/OpenControlApp.hpp>
#include <oc/api/ControlAPI.hpp>
#include <oc/context/IContext.hpp>
#include <oc/core/input/InputConfig.hpp>

// Drivers
#include <drivers/arduino/input/ButtonController.hpp>
#include <drivers/teensy/input/EncoderController.hpp>
#include <drivers/teensy/midi/TeensyUsbMidi.hpp>

// Local configuration
#include "Config.hpp"

// ═══════════════════════════════════════════════════════════════════
// Minimal Context Implementation
// ═══════════════════════════════════════════════════════════════════

/**
 * @brief Simple standalone context for MIDI controller
 *
 * Sets up all input bindings during initialization.
 * Encoders send CC, buttons toggle CC values.
 */
class MinimalContext : public oc::context::IContext {
public:
    bool initialize(oc::api::ControlAPI& api) override {
        api_ = &api;
        setupEncoderBindings();
        setupButtonBindings();
        Serial.println("[MinimalContext] Initialized");
        return true;
    }

    void update() override {
        // Nothing to do each frame for this simple context
    }

    void cleanup() override {
        Serial.println("[MinimalContext] Cleanup");
    }

    const char* getName() const override { return "Minimal Controller"; }
    const char* getId() const override { return "minimal"; }

private:
    void setupEncoderBindings() {
        // Encoder 1-4: Send MIDI CC on turn
        // Value is normalized [0.0-1.0], we map to [0-127]
        for (uint8_t i = 0; i < Config::ENCODERS.size(); ++i) {
            oc::hal::EncoderID id = Config::ENCODERS[i].id;
            uint8_t cc = Config::ENCODER_CC_BASE + i;

            api_->onTurned(id, [this, cc](float value) {
                uint8_t midiValue = static_cast<uint8_t>(value * 127.0f);
                api_->sendCC(Config::MIDI_CHANNEL, cc, midiValue);
                Serial.printf("[Encoder] CC %d = %d\n", cc, midiValue);
            });
        }
    }

    void setupButtonBindings() {
        // Button 1: Press sends CC 127, release sends CC 0
        api_->onPressed(Config::BUTTONS[0].id, [this]() {
            api_->sendCC(Config::MIDI_CHANNEL, Config::BUTTON1_CC, 127);
            Serial.println("[Button 1] Pressed -> CC 127");
        });

        api_->onReleased(Config::BUTTONS[0].id, [this]() {
            api_->sendCC(Config::MIDI_CHANNEL, Config::BUTTON1_CC, 0);
            Serial.println("[Button 1] Released -> CC 0");
        });

        // Button 1: Long press for alternative action
        api_->onLongPress(Config::BUTTONS[0].id, []() {
            Serial.println("[Button 1] Long press!");
            // Example: could trigger a different CC or mode switch
        }, Config::LONG_PRESS_MS);

        // Button 2: Toggle behavior (press sends 127, press again sends 0)
        api_->onPressed(Config::BUTTONS[1].id, [this]() {
            button2_state_ = !button2_state_;
            uint8_t value = button2_state_ ? 127 : 0;
            api_->sendCC(Config::MIDI_CHANNEL, Config::BUTTON2_CC, value);
            Serial.printf("[Button 2] Toggle -> CC %d\n", value);
        });
    }

    oc::api::ControlAPI* api_ = nullptr;
    bool button2_state_ = false;
};

// ═══════════════════════════════════════════════════════════════════
// Global Application Instance
// ═══════════════════════════════════════════════════════════════════

// Use std::optional because OpenControlApp requires AppBuilder for construction
std::optional<oc::app::OpenControlApp> app;

// ═══════════════════════════════════════════════════════════════════
// Arduino Setup
// ═══════════════════════════════════════════════════════════════════

void setup() {
    // Initialize serial for debug output
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {
        // Wait for serial (with timeout for standalone operation)
    }
    Serial.println("\n========================================");
    Serial.println("Open Control - Minimal Teensy 4.1 Example");
    Serial.println("========================================\n");

    // ─────────────────────────────────────────────────────
    // Configure input timing
    // ─────────────────────────────────────────────────────
    oc::core::InputConfig inputConfig{
        .longPressMs = Config::LONG_PRESS_MS,
        .doubleTapWindowMs = Config::DOUBLE_TAP_MS
    };

    // ─────────────────────────────────────────────────────
    // Build application with AppBuilder
    // ─────────────────────────────────────────────────────
    app.emplace(oc::app::AppBuilder()
        // Time provider (required) - platform-specific milliseconds function
        .timeProvider(millis)

        // MIDI transport
        .midi(std::make_unique<oc::drivers::teensy::TeensyUsbMidi>())

        // Encoders (4x using Teensy optimized driver with ISR)
        .encoders(std::make_unique<oc::drivers::teensy::EncoderController<4>>(Config::ENCODERS))

        // Buttons (2x using generic Arduino driver)
        .buttons(std::make_unique<oc::drivers::arduino::ButtonController<2>>(
            Config::BUTTONS,
            nullptr,  // No multiplexer (direct GPIO)
            Config::DEBOUNCE_MS
        ))

        // Input timing configuration
        .inputConfig(inputConfig)

        // Build the application
        .build());

    // ─────────────────────────────────────────────────────
    // Register context and start
    // ─────────────────────────────────────────────────────
    app->registerContext<MinimalContext>("minimal");

    if (!app->begin()) {
        Serial.println("[ERROR] Failed to initialize application!");
        while (true) {
            // Halt on error
            delay(1000);
        }
    }

    Serial.println("[OK] Application started successfully");
    Serial.println("\nControls:");
    Serial.println("  Encoders 1-4: Send MIDI CC 16-19");
    Serial.println("  Button 1: Press=CC20:127, Release=CC20:0, LongPress=debug");
    Serial.println("  Button 2: Toggle CC21 (127/0)");
    Serial.println("\n");
}

// ═══════════════════════════════════════════════════════════════════
// Arduino Loop
// ═══════════════════════════════════════════════════════════════════

void loop() {
    // Update the application (polls inputs, processes events, updates context)
    app->update();
}
