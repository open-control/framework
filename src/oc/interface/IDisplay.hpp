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

    /**
     * @brief Flush a framebuffer-backed region.
     *
     * Drivers that can accumulate multiple dirty regions should override this
     * and only redraw when redrawNow is true. The default preserves compatible
     * full-frame behavior for drivers without region support.
     *
     * @param frameBuffer Base pointer to the full framebuffer.
     * @param area Inclusive dirty rectangle in display coordinates.
     * @param frameStride Full framebuffer stride in pixels.
     * @param redrawNow True for the last region of the current LVGL refresh.
     */
    virtual void flushRegion(
        const void* frameBuffer,
        const Rect& area,
        uint16_t frameStride,
        bool redrawNow
    ) {
        (void)area;
        (void)frameStride;
        if (redrawNow) {
            flush(frameBuffer, {
                .x1 = 0,
                .y1 = 0,
                .x2 = static_cast<int32_t>(width()) - 1,
                .y2 = static_cast<int32_t>(height()) - 1,
            });
        }
    }

    virtual uint16_t width() const = 0;
    virtual uint16_t height() const = 0;
};

}  // namespace oc::interface
