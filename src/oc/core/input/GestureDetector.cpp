#include "GestureDetector.hpp"

namespace oc::core::input {

GestureDetector::GestureDetector(const InputConfig& config)
    : config_(config) {}

// ═══════════════════════════════════════════════════
// State Updates
// ═══════════════════════════════════════════════════

void GestureDetector::onButtonPress(ButtonID buttonId, uint32_t time) {
    if (buttonId >= MAX_BUTTONS) return;

    button_states_[buttonId] = true;
    button_press_time_[buttonId] = time;

    // Double tap detection: increment tap count if within window
    if (time - button_release_time_[buttonId] < config_.doubleTapWindowMs) {
        button_tap_count_[buttonId]++;
    } else {
        button_tap_count_[buttonId] = 1;
    }
}

void GestureDetector::onButtonRelease(ButtonID buttonId, uint32_t time) {
    if (buttonId >= MAX_BUTTONS) return;

    button_states_[buttonId] = false;
    button_release_time_[buttonId] = time;
    long_press_triggered_[buttonId] = false;
}

void GestureDetector::resetButton(ButtonID buttonId) {
    if (buttonId >= MAX_BUTTONS) return;

    button_states_[buttonId] = false;
    button_press_time_[buttonId] = 0;
    button_release_time_[buttonId] = 0;
    button_tap_count_[buttonId] = 0;
    long_press_triggered_[buttonId] = false;
}

void GestureDetector::reset() {
    button_states_.fill(false);
    button_press_time_.fill(0);
    button_release_time_.fill(0);
    button_tap_count_.fill(0);
    long_press_triggered_.fill(false);
}

// ═══════════════════════════════════════════════════
// Gesture Detection
// ═══════════════════════════════════════════════════

bool GestureDetector::checkLongPress(ButtonID buttonId, uint32_t now, uint32_t requiredDuration) {
    if (buttonId >= MAX_BUTTONS) return false;
    if (!button_states_[buttonId]) return false;
    if (long_press_triggered_[buttonId]) return false;

    const uint32_t duration = requiredDuration > 0 ? requiredDuration : config_.longPressMs;
    const uint32_t elapsed = now - button_press_time_[buttonId];

    return elapsed >= duration;
}

void GestureDetector::markLongPressTriggered(ButtonID buttonId) {
    if (buttonId < MAX_BUTTONS) {
        long_press_triggered_[buttonId] = true;
    }
}

bool GestureDetector::checkDoubleTap(ButtonID buttonId, uint32_t now, uint32_t windowMs) {
    if (buttonId >= MAX_BUTTONS) return false;
    if (button_tap_count_[buttonId] < 2) return false;

    const uint32_t window = windowMs > 0 ? windowMs : config_.doubleTapWindowMs;
    const uint32_t elapsed = now - button_release_time_[buttonId];

    return elapsed < window;
}

void GestureDetector::resetTapCount(ButtonID buttonId) {
    if (buttonId < MAX_BUTTONS) {
        button_tap_count_[buttonId] = 0;
    }
}

bool GestureDetector::isComboActive(ButtonID btn1, ButtonID btn2) const {
    return isPressed(btn1) && isPressed(btn2);
}

// ═══════════════════════════════════════════════════
// State Queries
// ═══════════════════════════════════════════════════

bool GestureDetector::isPressed(ButtonID buttonId) const {
    if (buttonId >= MAX_BUTTONS) return false;
    return button_states_[buttonId];
}

uint32_t GestureDetector::pressTime(ButtonID buttonId) const {
    if (buttonId >= MAX_BUTTONS) return 0;
    return button_press_time_[buttonId];
}

uint32_t GestureDetector::releaseTime(ButtonID buttonId) const {
    if (buttonId >= MAX_BUTTONS) return 0;
    return button_release_time_[buttonId];
}

uint8_t GestureDetector::tapCount(ButtonID buttonId) const {
    if (buttonId >= MAX_BUTTONS) return 0;
    return button_tap_count_[buttonId];
}

bool GestureDetector::longPressTriggered(ButtonID buttonId) const {
    if (buttonId >= MAX_BUTTONS) return false;
    return long_press_triggered_[buttonId];
}

}  // namespace oc::core::input
