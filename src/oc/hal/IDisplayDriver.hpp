#pragma once

#include "Types.hpp"

namespace oc::hal {

/**
 * @brief Interface for display hardware abstraction
 */
class IDisplayDriver {
public:
    virtual ~IDisplayDriver() = default;

    virtual void init() = 0;
    virtual void flush(const void* buffer, const Rect& area) = 0;
    virtual uint16_t width() const = 0;
    virtual uint16_t height() const = 0;

    using FlushCallback = void (*)(IDisplayDriver*);
    virtual void setFlushCallback(FlushCallback cb) { flush_cb_ = cb; }

protected:
    FlushCallback flush_cb_ = nullptr;
};

}  // namespace oc::hal
