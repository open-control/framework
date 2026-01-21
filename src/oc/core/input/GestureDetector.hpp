#pragma once

/**
 * @file GestureDetector.hpp
 * @brief Gesture detection logic extracted from InputBinding
 *
 * Handles detection of:
 * - Long press (hold button for duration)
 * - Double tap (rapid double press)
 * - Combo (two buttons pressed simultaneously)
 */

#include <array>
#include <vector>

#include <oc/Config.hpp>
#include <oc/core/input/Binding.hpp>
#include <oc/core/input/InputConfig.hpp>

namespace oc::core::input {

/**
 * @brief Detects complex button gestures
 *
 * This is an internal helper class used by InputBinding.
 * It encapsulates the state and logic for gesture detection.
 */
class GestureDetector {
public:
    /**
     * @brief Construct gesture detector
     * @param config Timing configuration for gestures
     */
    explicit GestureDetector(const InputConfig& config);

    // ═══════════════════════════════════════════════════
    // State Updates (called by InputBinding on events)
    // ═══════════════════════════════════════════════════

    /**
     * @brief Record button press
     * @param buttonId Button that was pressed
     * @param time Current time in milliseconds
     */
    void onButtonPress(ButtonID buttonId, uint32_t time);

    /**
     * @brief Record button release
     * @param buttonId Button that was released
     * @param time Current time in milliseconds
     */
    void onButtonRelease(ButtonID buttonId, uint32_t time);

    /**
     * @brief Reset state for a button (e.g., after binding cleared)
     */
    void resetButton(ButtonID buttonId);

    /**
     * @brief Reset all gesture state
     */
    void reset();

    // ═══════════════════════════════════════════════════
    // Gesture Detection
    // ═══════════════════════════════════════════════════

    /**
     * @brief Check if long press should trigger for a button
     * @param buttonId Button to check
     * @param now Current time
     * @param requiredDuration Duration required (0 = use config default)
     * @return true if long press duration exceeded and not yet triggered
     */
    bool checkLongPress(ButtonID buttonId, uint32_t now, uint32_t requiredDuration = 0);

    /**
     * @brief Mark long press as triggered (prevents re-trigger)
     */
    void markLongPressTriggered(ButtonID buttonId);

    /**
     * @brief Check if double tap should trigger for a button
     * @param buttonId Button to check
     * @param now Current time
     * @param windowMs Window in ms (0 = use config default)
     * @return true if this is a valid double tap
     */
    bool checkDoubleTap(ButtonID buttonId, uint32_t now, uint32_t windowMs = 0);

    /**
     * @brief Reset tap count after double tap triggered
     */
    void resetTapCount(ButtonID buttonId);

    /**
     * @brief Check if two buttons are both currently pressed
     */
    bool isComboActive(ButtonID btn1, ButtonID btn2) const;

    // ═══════════════════════════════════════════════════
    // State Queries
    // ═══════════════════════════════════════════════════

    bool isPressed(ButtonID buttonId) const;
    uint32_t pressTime(ButtonID buttonId) const;
    uint32_t releaseTime(ButtonID buttonId) const;
    uint8_t tapCount(ButtonID buttonId) const;
    bool longPressTriggered(ButtonID buttonId) const;

    const InputConfig& config() const { return config_; }

private:
    const InputConfig& config_;

    std::array<bool, MAX_BUTTONS> button_states_{};
    std::array<uint32_t, MAX_BUTTONS> button_press_time_{};
    std::array<uint32_t, MAX_BUTTONS> button_release_time_{};
    std::array<uint8_t, MAX_BUTTONS> button_tap_count_{};
    std::array<bool, MAX_BUTTONS> long_press_triggered_{};
};

}  // namespace oc::core::input
