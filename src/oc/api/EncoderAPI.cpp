#include "EncoderAPI.hpp"

#include <oc/core/input/InputBinding.hpp>

namespace oc::api {

EncoderAPI::EncoderAPI(core::input::InputBinding& binding, hal::IEncoderController& hw)
    : binding_(binding), hw_(hw) {}

core::input::EncoderBuilder EncoderAPI::encoder(hal::EncoderID id) {
    return core::input::EncoderBuilder(&binding_, id);
}

void EncoderAPI::clearBindings() {
    binding_.clearEncoderBindings();
}

void EncoderAPI::clearScope(core::ScopeID scope) {
    binding_.clearEncoderScope(scope);
}

float EncoderAPI::getPosition(hal::EncoderID id) const {
    return hw_.getPosition(id);
}

void EncoderAPI::setPosition(hal::EncoderID id, float value) {
    hw_.setPosition(id, value);
}

void EncoderAPI::setMode(hal::EncoderID id, hal::EncoderMode mode) {
    hw_.setMode(id, mode);
}

void EncoderAPI::setBounds(hal::EncoderID id, float min, float max) {
    hw_.setBounds(id, min, max);
}

void EncoderAPI::setDelta(hal::EncoderID id, float delta) {
    hw_.setDelta(id, delta);
}

void EncoderAPI::setDiscreteSteps(hal::EncoderID id, uint8_t steps) {
    hw_.setDiscreteSteps(id, steps);
}

void EncoderAPI::setContinuous(hal::EncoderID id) {
    hw_.setContinuous(id);
}

}  // namespace oc::api
