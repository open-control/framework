#pragma once

#include <oc/type/Result.hpp>
#include <oc/type/Ids.hpp>
#include <oc/type/Callbacks.hpp>

namespace oc::interface {

/**
 * @brief Encoder operating modes
 *
 * Determines how encoder values are processed and reported.
 * Each mode is suited to different use cases:
 *
 * | Mode       | Value Range            | Use Case                    |
 * |------------|------------------------|-----------------------------|
 * | NORMALIZED | [min, max] (default 0-1) | Volume, pan, bounded params |
 * | RAW        | Accumulated ticks      | Scrolling, unbounded values |
 * | RELATIVE   | ±delta per detent      | Fine adjustments, menus     |
 */
enum class EncoderMode : uint8_t {
    /**
     * @brief Absolute position normalized to bounds (default)
     *
     * Value range: [min, max] as set by setBounds() (default [0.0, 1.0])
     * Use case: Volume faders, pan pots, any bounded parameter
     *
     * Example:
     * @code
     * encoders().setMode(id, EncoderMode::NORMALIZED);
     * encoders().setBounds(id, 0.0f, 1.0f);
     * onEncoder(id).turn().then([](float v) {
     *     // v is in [0.0, 1.0]
     *     setVolume(v);
     * });
     * @endcode
     */
    NORMALIZED,

    /**
     * @brief Raw hardware ticks as accumulated position
     *
     * Value: Accumulated tick count (can be negative)
     * Use case: Scrolling, unbounded counters, debugging
     *
     * Example:
     * @code
     * encoders().setMode(id, EncoderMode::RAW);
     * onEncoder(id).turn().then([](float ticks) {
     *     scrollPosition_ += static_cast<int>(ticks);
     * });
     * @endcode
     */
    RAW,

    /**
     * @brief Relative delta per detent
     *
     * Value: Configured delta (via setDelta()) per detent, signed for direction
     * Use case: Incremental adjustments, menu navigation, fine control
     *
     * Example:
     * @code
     * encoders().setMode(id, EncoderMode::RELATIVE);
     * encoders().setDelta(id, 0.01f);  // 1% per detent
     * onEncoder(id).turn().then([](float delta) {
     *     // delta is +0.01 or -0.01 per detent
     *     value_ = std::clamp(value_ + delta, 0.0f, 1.0f);
     * });
     * @endcode
     */
    RELATIVE
};

/**
 * @brief Interface for encoder hardware abstraction
 */
class IEncoder {
public:
    virtual ~IEncoder() = default;

    /**
     * @brief Initialize encoder hardware
     * @return Result<void> - ok() on success, err() with ErrorCode on failure
     */
    virtual oc::type::Result<void> init() = 0;

    virtual void update() = 0;

    virtual float getPosition(oc::type::EncoderID id) const = 0;       ///< Value depends on mode
    virtual void setPosition(oc::type::EncoderID id, float value) = 0;  ///< Value depends on mode

    virtual void setMode(oc::type::EncoderID id, EncoderMode mode) = 0;
    virtual void setBounds(oc::type::EncoderID id, float min, float max) = 0;
    virtual void setDelta(oc::type::EncoderID id, float delta) = 0;  ///< Set delta per detent (relative mode)
    virtual void setDiscreteSteps(oc::type::EncoderID id, uint8_t steps) = 0;
    virtual void setDiscreteTicksPerStep(oc::type::EncoderID id, uint16_t ticksPerStep) = 0;
    virtual void setNormalizedTurns(oc::type::EncoderID id, float turns) = 0;
    virtual void setContinuous(oc::type::EncoderID id) = 0;

    virtual void setCallback(oc::type::EncoderCallback cb) = 0;
};

}  // namespace oc::interface
