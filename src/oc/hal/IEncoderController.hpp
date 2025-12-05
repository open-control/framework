#pragma once

#include "Types.hpp"

namespace oc::hal {

/// Encoder operating mode
enum class EncoderMode : uint8_t {
    NORMALIZED,  ///< Position [0.0-1.0] based on bounds (default)
    RAW,         ///< Raw hardware position (ticks as float)
    RELATIVE     ///< Delta per detent
};

/**
 * @brief Interface for encoder hardware abstraction
 */
class IEncoderController {
public:
    virtual ~IEncoderController() = default;

    /**
     * @brief Initialize encoder hardware
     * @return true if initialization succeeded, false on failure
     */
    virtual bool init() = 0;

    virtual void update() = 0;

    virtual float getPosition(EncoderID id) const = 0;       ///< Value depends on mode
    virtual void setPosition(EncoderID id, float value) = 0;  ///< Value depends on mode

    virtual void setMode(EncoderID id, EncoderMode mode) = 0;
    virtual void setBounds(EncoderID id, float min, float max) = 0;
    virtual void setDelta(EncoderID id, float delta) = 0;  ///< Set delta per detent (relative mode)
    virtual void setDiscreteSteps(EncoderID id, uint8_t steps) = 0;
    virtual void setContinuous(EncoderID id) = 0;

    virtual void setCallback(EncoderCallback cb) = 0;
};

}  // namespace oc::hal
