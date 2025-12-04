#pragma once

#include "Types.hpp"

namespace oc::hal {

/// Encoder operating mode
enum class EncoderMode : uint8_t {
    ABSOLUTE,  ///< Track absolute position (0-max)
    RELATIVE   ///< Track delta only
};

/**
 * @brief Interface for encoder hardware abstraction
 */
class IEncoderController {
public:
    virtual ~IEncoderController() = default;

    virtual void init() = 0;
    virtual void update() = 0;

    virtual int32_t getPosition(EncoderID id) const = 0;
    virtual void setPosition(EncoderID id, int32_t position) = 0;

    virtual void setMode(EncoderID id, EncoderMode mode) = 0;
    virtual void setBounds(EncoderID id, int32_t min, int32_t max) = 0;
    virtual void setDelta(EncoderID id, float delta) = 0;  ///< Set delta per detent (relative mode)
    virtual void setDiscreteSteps(EncoderID id, uint8_t steps) = 0;
    virtual void setContinuous(EncoderID id) = 0;

    virtual void setCallback(EncoderCallback cb) = 0;
};

}  // namespace oc::hal
