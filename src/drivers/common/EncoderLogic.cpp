#include "EncoderLogic.hpp"

#include <cmath>    // std::round, std::abs
#include <cstdlib>  // abs (int)

namespace oc::drivers::common {

EncoderLogic::EncoderLogic(const EncoderConfig& config) : config_(config) {
    // Initialize virtual range and position to middle for NORMALIZED mode
    virtual_range_ = calculateDefaultVirtualRange();
    position_ = virtual_range_ / 2;
    last_value_ = 0.5f;
}

std::optional<float> EncoderLogic::processNewPosition(int32_t newPosition) {
    if (newPosition == position_) {
        return std::nullopt;
    }

    int32_t delta = newPosition - position_;
    position_ = newPosition;

    if (mode_ == hal::EncoderMode::RELATIVE) {
        return handleRelativeMode(delta);
    } else {
        return handlePositionMode(newPosition);
    }
}

void EncoderLogic::setMode(hal::EncoderMode mode) {
    mode_ = mode;
    // Reset state when switching modes
    if (mode == hal::EncoderMode::RELATIVE) {
        accumulated_delta_ = 0;
    } else {
        // Reset to center position for NORMALIZED/RAW modes
        position_ = virtual_range_ / 2;
        last_value_ = 0.5f;
    }
}

void EncoderLogic::setBounds(float min, float max) {
    bounds_min_ = min;
    bounds_max_ = max;
}

void EncoderLogic::setDelta(float delta) {
    delta_per_detent_ = delta;
}

void EncoderLogic::setDiscreteSteps(uint8_t steps) {
    if (mode_ != hal::EncoderMode::NORMALIZED) return;  // Only for NORMALIZED mode

    discrete_steps_ = steps;
    last_quantized_value_ = -1.0f;  // Force re-emission on next change

    // Ensure enough resolution for reliable step detection
    recalculateVirtualRangeForDiscreteSteps();
}

void EncoderLogic::setContinuous() {
    setDiscreteSteps(0);
}

int32_t EncoderLogic::setPosition(float value) {
    last_value_ = value;

    if (mode_ == hal::EncoderMode::NORMALIZED) {
        // Convert value back to ticks
        float boundsRange = bounds_max_ - bounds_min_;
        float normalized = (boundsRange > 0.0f) ? (value - bounds_min_) / boundsRange : 0.0f;
        position_ = static_cast<int32_t>(normalized * virtual_range_);
        return position_;
    }

    return position_;
}

std::optional<float> EncoderLogic::handleRelativeMode(int32_t delta) {
    accumulated_delta_ += delta;

    if (std::abs(accumulated_delta_) < config_.stepsPerDetent) {
        return std::nullopt;
    }

    float step = (accumulated_delta_ > 0) ? delta_per_detent_ : -delta_per_detent_;
    accumulated_delta_ = 0;  // Total reset (not partial)

    return step;
}

std::optional<float> EncoderLogic::handlePositionMode(int32_t pos) {
    float value;

    if (mode_ == hal::EncoderMode::RAW) {
        value = static_cast<float>(pos);
    } else {
        // NORMALIZED mode
        value = computeNormalizedValue(pos);
    }

    // Apply quantization if discrete steps configured
    if (discrete_steps_ > 0) {
        float quantized = std::round(value * (discrete_steps_ - 1)) / (discrete_steps_ - 1);
        if (quantized == last_quantized_value_) {
            return std::nullopt;
        }
        last_quantized_value_ = quantized;
        value = quantized;
    }

    if (value == last_value_) {
        return std::nullopt;
    }

    last_value_ = value;
    return value;
}

float EncoderLogic::computeNormalizedValue(int32_t pos) const {
    // Clamp position
    if (pos < 0) pos = 0;
    if (pos > virtual_range_) pos = virtual_range_;

    float normalized = static_cast<float>(pos) / virtual_range_;
    float boundsRange = bounds_max_ - bounds_min_;
    return bounds_min_ + (normalized * boundsRange);
}

int32_t EncoderLogic::calculateDefaultVirtualRange() const {
    // PPR * stepsPerDetent = ticks per full revolution
    // Scale by rangeAngle to get ticks for configured rotation range
    int32_t ticksPerRevolution = config_.ppr * config_.stepsPerDetent;
    return static_cast<int32_t>(ticksPerRevolution * (config_.rangeAngle / 360.0f));
}

void EncoderLogic::recalculateVirtualRangeForDiscreteSteps() {
    // Ensure enough resolution for discrete steps (like Core)
    // Minimum 2 ticks per step for reliable detection
    constexpr float DISCRETE_VALUES_SENSITIVITY = 0.5f;

    int32_t defaultRange = calculateDefaultVirtualRange();
    int32_t minRangeForSteps = static_cast<int32_t>(discrete_steps_ / DISCRETE_VALUES_SENSITIVITY);

    virtual_range_ = (discrete_steps_ > 0 && minRangeForSteps > defaultRange)
        ? minRangeForSteps
        : defaultRange;

    // Sync position to new range while preserving normalized value
    position_ = static_cast<int32_t>(last_value_ * virtual_range_);
}

}  // namespace oc::drivers::common
