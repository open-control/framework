#pragma once

#include <cstdint>

#include <oc/type/Result.hpp>

namespace oc::interface {

/**
 * @brief Rectangle for display regions
 *
 * Uses int32_t for compatibility with LVGL 9 coordinates.
 */
struct Rect {
    int32_t x1;
    int32_t y1;
    int32_t x2;
    int32_t y2;
};

/**
 * @brief Interface for display hardware abstraction
 */
class IDisplay {
public:
    virtual ~IDisplay() = default;

    /**
     * @brief Initialize display hardware
     * @return Result<void> - ok() on success, err() with ErrorCode on failure
     */
    virtual oc::type::Result<void> init() = 0;

    virtual void flush(const void* buffer, const Rect& area) = 0;
    virtual uint16_t width() const = 0;
    virtual uint16_t height() const = 0;
};

}  // namespace oc::interface
