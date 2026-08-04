#pragma once

#include <atomic>
#include <cstdint>
#include <optional>

#include <config/PlatformCompat.hpp>
#include <oc/interface/IEncoder.hpp>
#include <oc/type/Ids.hpp>
#include <oc/type/Callbacks.hpp>

namespace oc::core::input {

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
    oc::type::EncoderID id;              ///< Unique encoder identifier
    uint16_t ppr = 24;              ///< Pulses per revolution (physical encoder spec)
    uint16_t rangeAngle = 270;      ///< Degrees of rotation for full [0..1] range
    uint8_t ticksPerEvent = 4;      ///< Ticks before event emission (4 = one detent)
    bool invertDirection = false;   ///< Invert rotation direction (hardware-dependent)
};

/**
 * @brief Shared encoder logic for all platforms
 *
 * Handles mode processing, accumulation, bounds mapping, and value emission.
 * Platform-specific drivers publish integer delta or position data and this
 * class computes when and what to emit from foreground.
 *
 * Two publication approaches are supported:
 * - publishDeltaFromISR(): Fixed integer publication for interrupt drivers.
 * - processNewPosition(): Foreground publication for polling drivers.
 *
 * Policy and floating-point work are performed only by
 * consumePublishedDeltas() (or its flush() convenience wrapper). One instance
 * supports one publication context and one foreground consumer.
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
     * @brief Publish a raw integer delta from an ISR
     *
     * This is the complete interrupt-side contract: one lock-free integer
     * load/add/store. It performs no mode, bounds, floating-point, allocation,
     * logging or callback work. Calls for one instance must come from a single
     * non-nesting publication context.
     *
     * @param delta Raw signed delta from encoder hardware
     */
    OC_ALWAYS_INLINE void publishDeltaFromISR(int32_t delta) noexcept {
        if (delta == 0) return;

        const uint32_t published = published_sequence_.load(std::memory_order_relaxed);
        published_sequence_.store(
            published + static_cast<uint32_t>(delta),
            std::memory_order_release);
    }

    /**
     * @brief Compatibility adapter for drivers using the old API
     *
     * New interrupt drivers must call publishDeltaFromISR(). This adapter is
     * leased only until the HAL consumer migration is complete.
     */
    void processDelta(int32_t delta);

    /**
     * @brief Process a new absolute position (polling-based)
     *
     * Computes and publishes the delta internally. Policy remains deferred to
     * foreground consumption.
     *
     * @param newPosition Raw tick position from hardware
     */
    void processNewPosition(int32_t newPosition);

    /**
     * @brief Consume one published snapshot and apply encoder policy
     *
     * Must be called from the owning foreground context. Publications observed
     * by the snapshot are applied exactly once; later publications remain for
     * the next call.
     *
     * @return Value to emit, or std::nullopt if no value changed
     */
    std::optional<float> consumePublishedDeltas();

    /**
     * @brief Foreground convenience wrapper for consumePublishedDeltas()
     *
     * Must be called from main loop (not ISR) to get value for callback.
     *
     * @return Value to emit, or std::nullopt if no value changed
     */
    std::optional<float> flush();

    /**
     * @brief Check whether the published and consumed cursors differ
     */
    bool hasPending() const {
        return published_sequence_.load(std::memory_order_acquire) !=
               consumed_sequence_.load(std::memory_order_acquire);
    }

    // ═══════════════════════════════════════════════════
    // Getters
    // ═══════════════════════════════════════════════════

    oc::type::EncoderID getId() const { return config_.id; }
    float getLastValue() const { return last_value_; }
    interface::EncoderMode getMode() const { return mode_; }
    int32_t getPosition() const { return position_; }

    // ═══════════════════════════════════════════════════
    // Configuration
    // ═══════════════════════════════════════════════════

    void setMode(interface::EncoderMode mode);
    void setBounds(float min, float max);
    void setDelta(float delta);
    void setDiscreteSteps(uint8_t steps);
    void setDiscreteTicksPerStep(uint16_t ticksPerStep);
    void setNormalizedTurns(float turns);
    void setContinuous();

    /**
     * @brief Set position value and sync internal state
     * @param value New position value (interpretation depends on mode)
     * @return Tick position to write back to hardware
     */
    int32_t setPosition(float value);

private:
    /**
     * @brief Decode a signed delta from modular cursor arithmetic
     */
    static int32_t decodeModularDelta(uint32_t encodedDelta);

    /**
     * @brief Apply one consumed delta in the foreground
     */
    std::optional<float> applyPublishedDelta(int32_t delta);

    /**
     * @brief Handle RELATIVE mode delta
     */
    std::optional<float> handleRelativeMode(int64_t delta);

    /**
     * @brief Handle NORMALIZED mode with ±1 movement (Core-compatible)
     */
    std::optional<float> handleNormalizedMode(int64_t delta);

    /**
     * @brief Handle RAW mode
     */
    std::optional<float> handleRawMode(int64_t delta);

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
     * @brief Calculate default virtual range in ticks for configured angle
     */
    int32_t calculateDefaultVirtualRange() const;

    /**
     * @brief Calculate virtual range in ticks for a target number of turns
     */
    int32_t calculateVirtualRangeForTurns(float turns) const;

    /**
     * @brief Calculate currently configured virtual range (override or hardware default)
     */
    int32_t calculateConfiguredVirtualRange() const;

    /**
     * @brief Recalculate virtual range for discrete steps
     */
    void recalculateVirtualRangeForDiscreteSteps();

    EncoderConfig config_;

    // Virtual range (may be adjusted for discrete steps)
    int32_t virtual_range_ = 0;
    float normalized_turns_ = 0.0f;

    // State
    interface::EncoderMode mode_ = interface::EncoderMode::NORMALIZED;
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
    uint16_t discrete_ticks_per_step_ = 2;
    float last_quantized_value_ = -1.0f;

    // Single-producer integer publication and foreground consume cursors.
    // The unsigned representation makes sequence wrap well-defined.
    std::atomic<uint32_t> published_sequence_{0};
    std::atomic<uint32_t> consumed_sequence_{0};
};

static_assert(std::atomic<uint32_t>::is_always_lock_free,
              "Encoder ISR publication requires lock-free 32-bit atomics");
static_assert(sizeof(EncoderLogic) == 64,
              "EncoderLogic scalar publication state must remain 64 bytes");

}  // namespace oc::core::input
