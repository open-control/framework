#include "EncoderLogic.hpp"

#include <algorithm>  // std::clamp
#include <cmath>      // std::round, std::abs

namespace oc::core::input {

EncoderLogic::EncoderLogic(const EncoderConfig& config) : config_(config) {
    virtual_range_ = calculateDefaultVirtualRange();
    position_ = virtual_range_ / 2;
    last_value_ = 0.5f;
}

// ═══════════════════════════════════════════════════
// Processing
// ═══════════════════════════════════════════════════

void EncoderLogic::processDelta(int32_t delta) {
    if (delta == 0) return;

    // Apply direction inversion if configured (hardware-dependent)
    if (config_.invertDirection) {
        delta = -delta;
    }

    switch (mode_) {
        case interface::EncoderMode::RELATIVE:
            handleRelativeMode(delta);
            break;
        case interface::EncoderMode::NORMALIZED:
            handleNormalizedMode(delta);
            break;
        case interface::EncoderMode::RAW:
            // RAW mode: accumulate position directly
            position_ += delta;
            handleRawMode(position_);
            break;
    }
}

void EncoderLogic::processNewPosition(int32_t newPosition) {
    if (newPosition == last_raw_position_) return;

    int32_t delta = newPosition - last_raw_position_;
    last_raw_position_ = newPosition;

    // Process accumulated delta step by step for ±1 behavior
    while (delta != 0) {
        int32_t step = (delta > 0) ? 1 : -1;
        delta -= step;
        processDelta(step);
    }
}

std::optional<float> EncoderLogic::flush() {
    if (!has_pending_.load(std::memory_order_acquire)) return std::nullopt;

    has_pending_.store(false, std::memory_order_relaxed);
    float value = pending_value_.load(std::memory_order_relaxed);
    last_value_ = value;
    return value;
}

// ═══════════════════════════════════════════════════
// Mode Handlers
// ═══════════════════════════════════════════════════

void EncoderLogic::handleRelativeMode(int32_t delta) {
    accumulated_delta_ += delta;

    if (std::abs(accumulated_delta_) < config_.ticksPerEvent) {
        return;
    }

    float step = (accumulated_delta_ > 0) ? delta_per_detent_ : -delta_per_detent_;
    accumulated_delta_ = 0;

    setPending(step);
}

void EncoderLogic::handleNormalizedMode(int32_t direction) {
    // Core-compatible: move by ±1 regardless of delta magnitude
    int32_t movement = (direction > 0) ? 1 : -1;
    position_ = std::clamp(position_ + movement, int32_t{0}, virtual_range_);

    float normalizedValue = computeNormalizedValue();

    if (normalizedValue == last_value_) return;
    last_value_ = normalizedValue;

    float valueToEmit = normalizedValue;
    if (applyQuantization(valueToEmit, valueToEmit)) {
        setPending(valueToEmit);
    }
}

void EncoderLogic::handleRawMode(int32_t pos) {
    float value = static_cast<float>(pos);

    if (value == last_value_) return;
    last_value_ = value;

    setPending(value);
}

// ═══════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════

float EncoderLogic::computeNormalizedValue() const {
    float normalized = static_cast<float>(position_) / static_cast<float>(virtual_range_);
    float boundsRange = bounds_max_ - bounds_min_;
    return bounds_min_ + (normalized * boundsRange);
}

bool EncoderLogic::applyQuantization(float value, float& outValue) {
    if (discrete_steps_ == 0) {
        outValue = value;
        return true;
    }

    // Normalize to [0, 1] before quantization
    float boundsRange = bounds_max_ - bounds_min_;
    float normalized = (boundsRange > 0.0f) ? (value - bounds_min_) / boundsRange : 0.0f;

    // Quantize in normalized space
    float quantizedNorm = std::round(normalized * (discrete_steps_ - 1)) / (discrete_steps_ - 1);

    // Map back to bounds space
    float quantized = bounds_min_ + quantizedNorm * boundsRange;

    if (quantized == last_quantized_value_) {
        return false;
    }

    last_quantized_value_ = quantized;
    outValue = quantized;
    return true;
}

void EncoderLogic::setPending(float value) {
    pending_value_.store(value, std::memory_order_relaxed);
    has_pending_.store(true, std::memory_order_release);
}

int32_t EncoderLogic::calculateDefaultVirtualRange() const {
    // Always use full quadrature resolution (x4) for maximum precision
    int32_t ticksPerRevolution = config_.ppr * FULL_QUADRATURE_MULTIPLIER;
    return static_cast<int32_t>(ticksPerRevolution * (config_.rangeAngle / 360.0f));
}

void EncoderLogic::recalculateVirtualRangeForDiscreteSteps() {
    constexpr float DISCRETE_VALUES_SENSITIVITY = 0.5f;

    int32_t defaultRange = calculateDefaultVirtualRange();
    int32_t minRangeForSteps = static_cast<int32_t>(discrete_steps_ / DISCRETE_VALUES_SENSITIVITY);

    virtual_range_ = (discrete_steps_ > 0 && minRangeForSteps > defaultRange)
        ? minRangeForSteps
        : defaultRange;

    position_ = static_cast<int32_t>(last_value_ * virtual_range_);
}

// ═══════════════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════════════

void EncoderLogic::setMode(interface::EncoderMode mode) {
    mode_ = mode;

    if (mode == interface::EncoderMode::RELATIVE) {
        accumulated_delta_ = 0;
    } else {
        position_ = virtual_range_ / 2;
        last_value_ = 0.5f;
    }

    has_pending_.store(false, std::memory_order_release);
}

void EncoderLogic::setBounds(float min, float max) {
    bounds_min_ = min;
    bounds_max_ = max;
}

void EncoderLogic::setDelta(float delta) {
    delta_per_detent_ = delta;
}

void EncoderLogic::setDiscreteSteps(uint8_t steps) {
    if (mode_ != interface::EncoderMode::NORMALIZED) return;

    discrete_steps_ = steps;
    last_quantized_value_ = -1.0f;

    recalculateVirtualRangeForDiscreteSteps();
}

void EncoderLogic::setContinuous() {
    setDiscreteSteps(0);
}

int32_t EncoderLogic::setPosition(float value) {
    if (mode_ == interface::EncoderMode::NORMALIZED) {
        // Clamp value to bounds for consistency with handleNormalizedMode
        float clampedValue = std::clamp(value, bounds_min_, bounds_max_);
        last_value_ = clampedValue;

        float boundsRange = bounds_max_ - bounds_min_;
        float normalized = (boundsRange > 0.0f) ? (clampedValue - bounds_min_) / boundsRange : 0.0f;
        position_ = static_cast<int32_t>(normalized * virtual_range_);
    } else {
        // RAW and RELATIVE modes: no bounds, store value directly
        last_value_ = value;
        if (mode_ == interface::EncoderMode::RAW) {
            position_ = static_cast<int32_t>(value);
        }
    }

    has_pending_.store(false, std::memory_order_release);
    return position_;
}

}  // namespace oc::core::input
