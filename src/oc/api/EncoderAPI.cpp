#include "EncoderAPI.hpp"

#include <oc/core/input/InputBinding.hpp>

namespace oc::api {

EncoderAPI::EncoderAPI(core::input::InputBinding& binding, interface::IEncoder& hw)
    : binding_(binding), hw_(hw) {}

core::input::EncoderBuilder EncoderAPI::encoder(EncoderID id) {
    return core::input::EncoderBuilder(&binding_, id);
}

void EncoderAPI::clearBindings() {
    binding_.clearEncoderBindings();
}

void EncoderAPI::clearScope(oc::ScopeID scope) {
    binding_.clearEncoderScope(scope);
}

float EncoderAPI::getPosition(EncoderID id) const {
    return hw_.getPosition(id);
}

void EncoderAPI::setPosition(EncoderID id, float value) {
    hw_.setPosition(id, value);
}

void EncoderAPI::setMode(EncoderID id, interface::EncoderMode mode) {
    hw_.setMode(id, mode);
}

void EncoderAPI::setBounds(EncoderID id, float min, float max) {
    hw_.setBounds(id, min, max);
}

void EncoderAPI::setDelta(EncoderID id, float delta) {
    hw_.setDelta(id, delta);
}

void EncoderAPI::setDiscreteSteps(EncoderID id, uint8_t steps) {
    hw_.setDiscreteSteps(id, steps);
}

void EncoderAPI::setContinuous(EncoderID id) {
    hw_.setContinuous(id);
}

}  // namespace oc::api
