#pragma once

#include <cstdint>
#include <optional>

#include <oc/hal/IEncoderController.hpp>
#include <oc/hal/Types.hpp>

namespace oc::drivers::common {

/// Full quadrature multiplier (x4 resolution)
constexpr uint8_t FULL_QUADRATURE_MULTIPLIER = 4;

/**
 * @brief Encoder configuration
 *
 * Full quadrature resolution (x4) is always used for NORMALIZED mode range
 * calculation, providing maximum resolution regardless of ticksPerEvent setting.
 *
 * @note The `invertDirection` flag allows adapting to different hardware wiring
 *       without requiring physical rewiring. Encoder direction depends on:
 *       - Pin A/B wiring order
 *       - Encoder internal construction
 *       - Pull-up/pull-down configuration
 */
struct EncoderConfig {
    hal::EncoderID id;              ///< Unique encoder identifier
    uint16_t ppr = 24;              ///< Pulses per revolution (physical encoder spec)
    uint16_t rangeAngle = 270;      ///< Degrees of rotation for full [0..1] range
    uint8_t ticksPerEvent = 4;      ///< Ticks before event emission (4 = one detent)
    bool invertDirection = false;   ///< Invert rotation direction (hardware-dependent)
};

/**
 * @brief Shared encoder logic for all platforms
 *
 * Handles mode processing, accumulation, bounds mapping, and value emission.
 * Platform-specific drivers feed delta or position data and this class computes
 * when and what to emit.
 *
 * Two processing approaches are supported:
 * - processDelta(): For ISR-based drivers (Teensy). Processes ±1 per call like Core.
 * - processNewPosition(): For polling-based drivers (Arduino).
 *
 * Both use the pending pattern: values are stored internally and emitted via flush().
 * This prevents crashes from calling callbacks in ISR context.
 *
 * @note This class does NOT handle hardware - it only processes position changes.
 */
class EncoderLogic {
public:
    explicit EncoderLogic(const EncoderConfig& config);

    // ═══════════════════════════════════════════════════
    // Processing (choose one approach per driver)
    // ═══════════════════════════════════════════════════

    /**
     * @brief Process a delta from ISR callback (Core-compatible)
     *
     * For NORMALIZED mode: moves virtual position by ±1 (not raw delta).
     * For RELATIVE mode: accumulates delta, emits when threshold reached.
     *
     * Sets pending value internally. Call flush() from main loop to emit.
     *
     * @param delta Raw delta from encoder hardware (sign indicates direction)
     */
    void processDelta(int32_t delta);

    /**
     * @brief Process a new absolute position (polling-based)
     *
     * Computes delta internally and processes. Sets pending if value changed.
     *
     * @param newPosition Raw tick position from hardware
     */
    void processNewPosition(int32_t newPosition);

    /**
     * @brief Flush pending value for emission
     *
     * Must be called from main loop (not ISR) to get value for callback.
     *
     * @return Value to emit, or std::nullopt if no pending value
     */
    std::optional<float> flush();

    /**
     * @brief Check if there's a pending value
     */
    bool hasPending() const { return has_pending_; }

    // ═══════════════════════════════════════════════════
    // Getters
    // ═══════════════════════════════════════════════════

    hal::EncoderID getId() const { return config_.id; }
    float getLastValue() const { return last_value_; }
    hal::EncoderMode getMode() const { return mode_; }
    int32_t getPosition() const { return position_; }

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
     * @return Tick position to write back to hardware
     */
    int32_t setPosition(float value);

private:
    /**
     * @brief Handle RELATIVE mode delta
     */
    void handleRelativeMode(int32_t delta);

    /**
     * @brief Handle NORMALIZED mode with ±1 movement (Core-compatible)
     */
    void handleNormalizedMode(int32_t direction);

    /**
     * @brief Handle RAW mode
     */
    void handleRawMode(int32_t pos);

    /**
     * @brief Compute normalized value from virtual position
     */
    float computeNormalizedValue() const;

    /**
     * @brief Apply quantization and check if value changed
     * @return true if value should be emitted
     */
    bool applyQuantization(float value, float& outValue);

    /**
     * @brief Set pending value for emission
     */
    void setPending(float value);

    /**
     * @brief Calculate default virtual range in ticks for configured angle
     */
    int32_t calculateDefaultVirtualRange() const;

    /**
     * @brief Recalculate virtual range for discrete steps
     */
    void recalculateVirtualRangeForDiscreteSteps();

    EncoderConfig config_;

    // Virtual range (may be adjusted for discrete steps)
    int32_t virtual_range_ = 0;

    // State
    hal::EncoderMode mode_ = hal::EncoderMode::NORMALIZED;
    int32_t position_ = 0;              ///< Virtual position for NORMALIZED, raw for others
    int32_t last_raw_position_ = 0;     ///< Last raw position (for processNewPosition)
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

    // Pending pattern (prevents ISR callback crash)
    bool has_pending_ = false;
    float pending_value_ = 0.0f;
};

}  // namespace oc::drivers::common
