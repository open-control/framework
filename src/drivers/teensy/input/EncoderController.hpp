#pragma once

#include <memory>
#include <vector>

#include <EncoderTool.h>

#include <drivers/common/EncoderLogic.hpp>
#include <oc/hal/IEncoderController.hpp>
#include <oc/hal/Types.hpp>

namespace oc::drivers::teensy {

/**
 * @brief Encoder definition for controller configuration
 *
 * @note See common::EncoderConfig for parameter documentation.
 *       invertDirection allows adapting to different hardware wiring.
 */
struct EncoderDef {
    hal::EncoderID id;              ///< Unique encoder identifier
    uint8_t pinA;                   ///< Quadrature signal A pin
    uint8_t pinB;                   ///< Quadrature signal B pin
    uint16_t ppr = 24;              ///< Pulses per revolution
    uint16_t rangeAngle = 270;      ///< Degrees for full [0..1] range
    uint8_t ticksPerEvent = 4;      ///< Ticks before event emission (4 = one detent)
    bool invertDirection = false;   ///< Invert rotation direction
};

/**
 * @brief Teensy encoder controller using EncoderTool library (ISR-based)
 */
class EncoderController : public hal::IEncoderController {
public:
    explicit EncoderController(const std::vector<EncoderDef>& defs);
    ~EncoderController() override = default;

    EncoderController(const EncoderController&) = delete;
    EncoderController& operator=(const EncoderController&) = delete;
    EncoderController(EncoderController&&) = delete;
    EncoderController& operator=(EncoderController&&) = delete;

    bool init() override;
    void update() override;

    float getPosition(hal::EncoderID id) const override;
    void setPosition(hal::EncoderID id, float value) override;
    void setMode(hal::EncoderID id, hal::EncoderMode mode) override;
    void setBounds(hal::EncoderID id, float min, float max) override;
    void setDiscreteSteps(hal::EncoderID id, uint8_t steps) override;
    void setContinuous(hal::EncoderID id) override;
    void setDelta(hal::EncoderID id, float delta) override;
    void setCallback(hal::EncoderCallback cb) override;

private:
    int findIndex(hal::EncoderID id) const;

    std::vector<EncoderDef> defs_;
    std::vector<std::unique_ptr<EncoderTool::Encoder>> encoders_hw_;
    std::vector<std::unique_ptr<common::EncoderLogic>> encoders_logic_;
    hal::EncoderCallback callback_;
    bool initialized_ = false;
};

}  // namespace oc::drivers::teensy
