#pragma once

#include <cstdint>
#include <functional>

#include <oc/core/input/Binding.hpp>
#include <oc/type/Ids.hpp>

namespace oc::core::input {

enum class InputBindingTraceStage : uint8_t {
    Event,
    Candidate,
    Dispatch,
    NoDispatch,
};

enum class InputBindingTraceDomain : uint8_t {
    Button,
    Encoder,
};

struct InputBindingTraceEvent {
    InputBindingTraceStage stage = InputBindingTraceStage::Event;
    InputBindingTraceDomain domain = InputBindingTraceDomain::Button;
    oc::type::ButtonID buttonId = 0;
    oc::type::EncoderID encoderId = 0;
    ButtonBindingType buttonType = ButtonBindingType::PRESS;
    EncoderBindingType encoderType = EncoderBindingType::TURN;
    oc::type::BindingID bindingId = 0;
    oc::type::ScopeID scopeId = 0;
    oc::type::ScopeID authorityScope = 0;
    bool scoped = false;
    bool active = false;
    bool authority = false;
    bool requiredButton = true;
    bool dispatched = false;
    float encoderValue = 0.0f;
};

using InputBindingTraceCallback = std::function<void(const InputBindingTraceEvent&)>;

}  // namespace oc::core::input
