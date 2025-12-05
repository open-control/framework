#include "EncoderController.hpp"

namespace oc::drivers::teensy {

EncoderController::EncoderController(const std::vector<EncoderDef>& defs) : defs_(defs) {
    encoders_hw_.reserve(defs_.size());
    encoders_logic_.reserve(defs_.size());

    for (const auto& def : defs_) {
        common::EncoderConfig cfg{
            .id = def.id,
            .ppr = def.ppr,
            .rangeAngle = def.rangeAngle,
            .ticksPerEvent = def.ticksPerEvent,
            .invertDirection = def.invertDirection
        };
        encoders_logic_.push_back(std::make_unique<common::EncoderLogic>(cfg));
    }
}

bool EncoderController::init() {
    if (initialized_) return true;

    for (size_t i = 0; i < defs_.size(); ++i) {
        const auto& def = defs_[i];

        encoders_hw_.push_back(std::make_unique<EncoderTool::Encoder>());
        encoders_hw_[i]->begin(def.pinA, def.pinB, EncoderTool::CountMode::full);

        encoders_hw_[i]->attachCallback([this, i](int, int delta) {
            encoders_logic_[i]->processDelta(delta);
        });
    }

    initialized_ = true;
    return true;
}

void EncoderController::update() {
    if (!initialized_) return;

    for (size_t i = 0; i < defs_.size(); ++i) {
        auto pending = encoders_logic_[i]->flush();
        if (pending.has_value() && callback_) {
            callback_(defs_[i].id, pending.value());
        }
    }
}

int EncoderController::findIndex(hal::EncoderID id) const {
    for (size_t i = 0; i < defs_.size(); ++i) {
        if (defs_[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

float EncoderController::getPosition(hal::EncoderID id) const {
    int idx = findIndex(id);
    if (idx < 0) return 0.0f;
    return encoders_logic_[idx]->getLastValue();
}

void EncoderController::setPosition(hal::EncoderID id, float value) {
    int idx = findIndex(id);
    if (idx >= 0) encoders_logic_[idx]->setPosition(value);
}

void EncoderController::setMode(hal::EncoderID id, hal::EncoderMode mode) {
    int idx = findIndex(id);
    if (idx >= 0) encoders_logic_[idx]->setMode(mode);
}

void EncoderController::setBounds(hal::EncoderID id, float min, float max) {
    int idx = findIndex(id);
    if (idx >= 0) encoders_logic_[idx]->setBounds(min, max);
}

void EncoderController::setDiscreteSteps(hal::EncoderID id, uint8_t steps) {
    int idx = findIndex(id);
    if (idx >= 0) encoders_logic_[idx]->setDiscreteSteps(steps);
}

void EncoderController::setContinuous(hal::EncoderID id) {
    int idx = findIndex(id);
    if (idx >= 0) encoders_logic_[idx]->setContinuous();
}

void EncoderController::setDelta(hal::EncoderID id, float delta) {
    int idx = findIndex(id);
    if (idx >= 0) encoders_logic_[idx]->setDelta(delta);
}

void EncoderController::setCallback(hal::EncoderCallback cb) {
    callback_ = cb;
}

}  // namespace oc::drivers::teensy
