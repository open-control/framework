#pragma once

#include <cstdint>
#include <optional>

#include <oc/hal/IEncoderController.hpp>
#include <oc/hal/Types.hpp>

namespace oc::drivers::common {

/**
 * @brief Encoder configuration
 */
struct EncoderConfig {
    hal::EncoderID id;           ///< Unique encoder identifier
    uint16_t ppr = 24;           ///< Pulses per revolution
    uint8_t stepsPerDetent = 4;  ///< Steps per physical detent click
    uint16_t rangeAngle = 270;   ///< Degrees of rotation for full [0..1] range
};

/**
 * @brief Shared encoder logic for all platforms
 *
 * Handles mode processing, accumulation, bounds mapping, and value emission.
 * Platform-specific drivers feed raw position data and this class computes
 * when and what to emit.
 *
 * This class is used by both Arduino and Teensy EncoderController implementations
 * to ensure consistent behavior across platforms.
 *
 * @note This class does NOT handle hardware - it only processes position changes.
 */
class EncoderLogic {
public:
    explicit EncoderLogic(const EncoderConfig& config);

    /**
     * @brief Process a new raw position from hardware
     *
     * Call this when the hardware encoder position changes.
     * Returns the value to emit if an event should be triggered.
     *
     * @param newPosition Raw tick position from hardware
     * @return Value to emit, or std::nullopt if no emission needed
     */
    std::optional<float> processNewPosition(int32_t newPosition);

    // ═══════════════════════════════════════════════════
    // Getters
    // ═══════════════════════════════════════════════════

    hal::EncoderID getId() const { return config_.id; }
    float getLastValue() const { return last_value_; }
    hal::EncoderMode getMode() const { return mode_; }

    // ═══════════════════════════════════════════════════
    // Configuration
    // ═══════════════════════════════════════════════════

    void setMode(hal::EncoderMode mode);
    void setBounds(float min, float max);
    void setDelta(float delta);
    void setDiscreteSteps(uint8_t steps);
    void setContinuous();

    /**
     * @brief Set position value and sync internal state
     * @param value New position value (interpretation depends on mode)
     * @return Tick position to write back to hardware (for NORMALIZED mode sync)
     */
    int32_t setPosition(float value);

private:
    /**
     * @brief Handle RELATIVE mode - accumulate and emit per detent
     */
    std::optional<float> handleRelativeMode(int32_t delta);

    /**
     * @brief Handle NORMALIZED/RAW modes - emit on position change
     */
    std::optional<float> handlePositionMode(int32_t pos);

    /**
     * @brief Compute value for NORMALIZED mode
     */
    float computeNormalizedValue(int32_t pos) const;

    /**
     * @brief Calculate default virtual range in ticks for configured angle
     */
    int32_t calculateDefaultVirtualRange() const;

    /**
     * @brief Recalculate virtual range for discrete steps (ensures enough resolution)
     */
    void recalculateVirtualRangeForDiscreteSteps();

    EncoderConfig config_;

    // Virtual range (may be adjusted for discrete steps)
    int32_t virtual_range_ = 0;

    // State
    hal::EncoderMode mode_ = hal::EncoderMode::NORMALIZED;
    int32_t position_ = 0;
    float last_value_ = 0.5f;

    // Bounds
    float bounds_min_ = 0.0f;
    float bounds_max_ = 1.0f;

    // RELATIVE mode
    int32_t accumulated_delta_ = 0;
    float delta_per_detent_ = 1.0f;

    // Discrete steps (quantization)
    uint8_t discrete_steps_ = 0;
    float last_quantized_value_ = -1.0f;
};

}  // namespace oc::drivers::common
