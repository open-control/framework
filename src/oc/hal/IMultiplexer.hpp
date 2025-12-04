#pragma once

#include <cstdint>

namespace oc::hal {

/**
 * @brief Interface for multiplexer hardware abstraction
 */
class IMultiplexer {
public:
    virtual ~IMultiplexer() = default;

    virtual void init() = 0;
    virtual void select(uint8_t channel) = 0;
    virtual bool readChannel(uint8_t channel) = 0;
    virtual uint8_t channelCount() const = 0;
};

}  // namespace oc::hal
