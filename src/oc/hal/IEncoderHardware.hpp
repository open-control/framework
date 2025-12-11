#pragma once

#include <cstdint>
#include <memory>

#include <oc/core/Result.hpp>

namespace oc::hal {

/**
 * @brief Callback signature for encoder delta events
 *
 * @param context User-provided context pointer (typically EncoderContext*)
 * @param delta Direction and magnitude of rotation (typically +1 or -1 per detent)
 *
 * @note Uses void* instead of std::function for ISR safety (zero allocation)
 */
using EncoderDeltaCallback = void (*)(void* context, int32_t delta);

/**
 * @brief Interface for encoder hardware abstraction (one instance per physical encoder)
 *
 * Responsibility: Read hardware and signal deltas to the controller.
 * Does NOT handle mode logic (that's EncoderLogic in core/).
 *
 * Implementations:
 * - Teensy: EncoderTool (ISR-based, hardware quadrature)
 * - ESP32: pcnt peripheral or polling
 * - STM32: Timer encoder mode or polling
 */
class IEncoderHardware {
public:
    virtual ~IEncoderHardware() = default;

    /**
     * @brief Initialize encoder hardware (pins, ISR, etc.)
     * @return Result<void> - ok() on success, err() with ErrorCode on failure
     */
    virtual core::Result<void> init() = 0;

    /**
     * @brief Configure the delta callback
     *
     * @param callback Function called on each delta (may be ISR context)
     * @param context Pointer passed to callback (typically EncoderContext*)
     *
     * @note Callback must be set BEFORE init() for ISR-based implementations
     */
    virtual void setDeltaCallback(EncoderDeltaCallback callback, void* context) = 0;
};

/**
 * @brief Factory for creating encoder hardware instances
 *
 * Allows HALs to provide their implementation without exposing concrete types.
 */
class IEncoderHardwareFactory {
public:
    virtual ~IEncoderHardwareFactory() = default;

    /**
     * @brief Create an encoder hardware instance
     * @param pinA First quadrature pin
     * @param pinB Second quadrature pin
     * @return Unique pointer to encoder hardware
     */
    virtual std::unique_ptr<IEncoderHardware> create(uint8_t pinA, uint8_t pinB) = 0;
};

}  // namespace oc::hal
