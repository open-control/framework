#include "EncoderLogic.hpp"

#include <algorithm>  // std::clamp
#include <cmath>      // std::round
#include <cstdint>
#include <limits>

#include <config/PlatformCompat.hpp>

namespace oc::core::input {

EncoderLogic::EncoderLogic(const EncoderConfig& config) : config_(config) {
    virtual_range_ = calculateConfiguredVirtualRange();
    position_ = virtual_range_ / 2;
    last_value_ = 0.5f;
}

// ═══════════════════════════════════════════════════
// Processing
// ═══════════════════════════════════════════════════

void EncoderLogic::processDelta(int32_t delta) {
    publishDeltaFromISR(delta);
}

void EncoderLogic::processNewPosition(int32_t newPosition) {
    if (newPosition == last_raw_position_) return;

    const uint32_t encodedDelta = static_cast<uint32_t>(newPosition) -
                                  static_cast<uint32_t>(last_raw_position_);
    last_raw_position_ = newPosition;
    publishDeltaFromISR(decodeModularDelta(encodedDelta));
}

int32_t EncoderLogic::decodeModularDelta(uint32_t encodedDelta) {
    if (encodedDelta <= static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
        return static_cast<int32_t>(encodedDelta);
    }

    return static_cast<int32_t>(
        static_cast<int64_t>(encodedDelta) - (int64_t{1} << 32));
}

std::optional<float> EncoderLogic::consumePublishedDeltas() {
    const uint32_t consumed = consumed_sequence_.load(std::memory_order_relaxed);
    const uint32_t published = published_sequence_.load(std::memory_order_acquire);
    if (published == consumed) return std::nullopt;

    consumed_sequence_.store(published, std::memory_order_release);
    return applyPublishedDelta(decodeModularDelta(published - consumed));
}

std::optional<float> EncoderLogic::flush() {
    return consumePublishedDeltas();
}

FLASHMEM std::optional<float> EncoderLogic::applyPublishedDelta(int32_t delta) {
    if (delta == 0) return std::nullopt;

    int64_t adjustedDelta = delta;
    if (config_.invertDirection) {
        adjustedDelta = -adjustedDelta;
    }

    switch (mode_) {
        case interface::EncoderMode::RELATIVE:
            return handleRelativeMode(adjustedDelta);
        case interface::EncoderMode::NORMALIZED:
            return handleNormalizedMode(adjustedDelta);
        case interface::EncoderMode::RAW:
            return handleRawMode(adjustedDelta);
    }

    return std::nullopt;
}

// ═══════════════════════════════════════════════════
// Mode Handlers
// ═══════════════════════════════════════════════════

FLASHMEM std::optional<float> EncoderLogic::handleRelativeMode(int64_t delta) {
    const int64_t ticksPerEvent = std::max<int64_t>(1, config_.ticksPerEvent);
    const int64_t total = static_cast<int64_t>(accumulated_delta_) + delta;
    const int64_t detents = total / ticksPerEvent;
    accumulated_delta_ = static_cast<int32_t>(total % ticksPerEvent);

    if (detents == 0) return std::nullopt;

    const float value = static_cast<float>(detents) * delta_per_detent_;
    last_value_ = value;
    return value;
}

FLASHMEM std::optional<float> EncoderLogic::handleNormalizedMode(int64_t delta) {
    const int64_t nextPosition = std::clamp(
        static_cast<int64_t>(position_) + delta,
        int64_t{0},
        static_cast<int64_t>(virtual_range_));
    position_ = static_cast<int32_t>(nextPosition);

    float normalizedValue = computeNormalizedValue();

    if (normalizedValue == last_value_) return std::nullopt;
    last_value_ = normalizedValue;

    float valueToEmit = normalizedValue;
    if (applyQuantization(valueToEmit, valueToEmit)) {
        last_value_ = valueToEmit;
        return valueToEmit;
    }

    return std::nullopt;
}

FLASHMEM std::optional<float> EncoderLogic::handleRawMode(int64_t delta) {
    const int64_t nextPosition = std::clamp(
        static_cast<int64_t>(position_) + delta,
        static_cast<int64_t>(std::numeric_limits<int32_t>::min()),
        static_cast<int64_t>(std::numeric_limits<int32_t>::max()));
    position_ = static_cast<int32_t>(nextPosition);
    const float value = static_cast<float>(position_);

    if (value == last_value_) return std::nullopt;
    last_value_ = value;

    return value;
}

// ═══════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════

FLASHMEM float EncoderLogic::computeNormalizedValue() const {
    float normalized = static_cast<float>(position_) / static_cast<float>(virtual_range_);
    float boundsRange = bounds_max_ - bounds_min_;
    return bounds_min_ + (normalized * boundsRange);
}

FLASHMEM bool EncoderLogic::applyQuantization(float value, float& outValue) {
    if (discrete_steps_ == 0) {
        outValue = value;
        return true;
    }

    // A one-value control is intentionally fixed. Treat it as a valid
    // non-emitting configuration instead of dividing by (steps - 1) below.
    // This is used by read-only/non-applicable rows that still share a
    // normalized encoder with editable controls.
    if (discrete_steps_ == 1) {
        outValue = bounds_min_;
        last_value_ = outValue;
        last_quantized_value_ = outValue;
        return false;
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

int32_t EncoderLogic::calculateDefaultVirtualRange() const {
    return calculateVirtualRangeForTurns(config_.rangeAngle / 360.0f);
}

int32_t EncoderLogic::calculateVirtualRangeForTurns(float turns) const {
    // Always use full quadrature resolution (x4) for maximum precision
    int32_t ticksPerRevolution = config_.ppr * FULL_QUADRATURE_MULTIPLIER;
    return static_cast<int32_t>(ticksPerRevolution * turns);
}

int32_t EncoderLogic::calculateConfiguredVirtualRange() const {
    if (normalized_turns_ > 0.0f) {
        return calculateVirtualRangeForTurns(normalized_turns_);
    }

    return calculateDefaultVirtualRange();
}

void EncoderLogic::recalculateVirtualRangeForDiscreteSteps() {
    int32_t defaultRange = calculateConfiguredVirtualRange();
    int32_t minRangeForSteps = static_cast<int32_t>(discrete_steps_) *
                               static_cast<int32_t>(discrete_ticks_per_step_);

    virtual_range_ = (discrete_steps_ > 0 && minRangeForSteps > defaultRange)
        ? minRangeForSteps
        : defaultRange;

    float boundsRange = bounds_max_ - bounds_min_;
    float normalized = (boundsRange > 0.0f) ? (last_value_ - bounds_min_) / boundsRange : 0.0f;
    normalized = std::clamp(normalized, 0.0f, 1.0f);
    position_ = static_cast<int32_t>(normalized * virtual_range_);
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

void EncoderLogic::setDiscreteTicksPerStep(uint16_t ticksPerStep) {
    discrete_ticks_per_step_ = std::max<uint16_t>(1, ticksPerStep);

    if (mode_ == interface::EncoderMode::NORMALIZED && discrete_steps_ > 0) {
        recalculateVirtualRangeForDiscreteSteps();
    }
}

void EncoderLogic::setNormalizedTurns(float turns) {
    normalized_turns_ = std::max(0.0f, turns);

    if (mode_ != interface::EncoderMode::NORMALIZED) return;

    if (discrete_steps_ > 0) {
        recalculateVirtualRangeForDiscreteSteps();
        return;
    }

    virtual_range_ = calculateConfiguredVirtualRange();

    float boundsRange = bounds_max_ - bounds_min_;
    float normalized = (boundsRange > 0.0f) ? (last_value_ - bounds_min_) / boundsRange : 0.0f;
    normalized = std::clamp(normalized, 0.0f, 1.0f);
    position_ = static_cast<int32_t>(normalized * virtual_range_);
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

    return position_;
}

}  // namespace oc::core::input
