#pragma once

#include <cstdint>

#include <oc/type/Result.hpp>

namespace oc::interface {

/**
 * @brief Interface for multiplexer hardware abstraction
 *
 * Supports both digital (buttons) and analog (pots/faders) reads.
 */
class IMultiplexer {
public:
    virtual ~IMultiplexer() = default;

    /**
     * @brief Initialize multiplexer hardware
     * @return Result<void> - ok() on success, err() with ErrorCode on failure
     */
    virtual oc::type::Result<void> init() = 0;

    /// Select a channel (for advanced batch reading)
    virtual void select(uint8_t channel) = 0;

    /// Read channel as digital (select + read)
    virtual bool readDigital(uint8_t channel) = 0;

    /// Read channel as analog (optional, returns 0 if not supported)
    virtual uint16_t readAnalog(uint8_t channel) { return 0; }

    /// Does this mux support analog reading?
    virtual bool supportsAnalog() const { return false; }

    /// Get total number of channels
    virtual uint8_t channelCount() const = 0;
};

}  // namespace oc::interface
