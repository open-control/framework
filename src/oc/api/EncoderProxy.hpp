#pragma once

#include <oc/api/EncoderAPI.hpp>

namespace oc::api {

/**
 * @brief Lightweight proxy for single encoder state access
 *
 * Created by IContext::encoder(id), provides fluent access to
 * a specific encoder's state without repeating the ID.
 *
 * @code
 * // Via IContext:
 * encoder(ENC_1).setPosition(0.5f);
 * encoder(ENC_1).setMode(EncoderMode::RELATIVE);
 * encoder(ENC_1).setDelta(0.01f);
 * float pos = encoder(ENC_1).position();
 * @endcode
 */
class EncoderProxy {
public:
    EncoderProxy(EncoderAPI& api, hal::EncoderID id) : api_(api), id_(id) {}

    /// Get current encoder position
    float position() const { return api_.getPosition(id_); }

    /// Set encoder position
    void setPosition(float value) { api_.setPosition(id_, value); }

    /// Set encoder operating mode
    void setMode(hal::EncoderMode mode) { api_.setMode(id_, mode); }

    /// Set encoder bounds for absolute mode
    void setBounds(float min, float max) { api_.setBounds(id_, min, max); }

    /// Set delta per detent for relative mode
    void setDelta(float delta) { api_.setDelta(id_, delta); }

    /// Configure encoder for discrete steps
    void setDiscreteSteps(uint8_t steps) { api_.setDiscreteSteps(id_, steps); }

    /// Configure encoder for continuous mode
    void setContinuous() { api_.setContinuous(id_); }

private:
    EncoderAPI& api_;
    hal::EncoderID id_;
};

}  // namespace oc::api
