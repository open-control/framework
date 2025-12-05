#if __has_include(<lvgl.h>)

#include "LVGLBridge.hpp"

namespace oc::ui {

LVGLBridge::LVGLBridge(const LVGLBridgeConfig& config) : config_(config) {}

LVGLBridge::~LVGLBridge() {
    if (display_) {
        lv_display_delete(display_);
        display_ = nullptr;
    }
}

bool LVGLBridge::init() {
    if (initialized_) return true;

    // Validate required config
    if (!config_.driver) return false;
    if (!config_.buffer1) return false;
    if (config_.bufferSizeBytes == 0) return false;

    // Create display
    display_ = lv_display_create(config_.width, config_.height);
    if (!display_) return false;

    // Set draw buffers
    lv_display_set_buffers(
        display_,
        config_.buffer1,
        config_.buffer2,
        config_.bufferSizeBytes,
        config_.renderMode
    );

    // Set color format (RGB565 for ILI9341 and similar displays)
    lv_display_set_color_format(display_, LV_COLOR_FORMAT_RGB565);

    // Wire flush callback to our display driver
    lv_display_set_flush_cb(display_, flushCallback);
    lv_display_set_user_data(display_, config_.driver);

    initialized_ = true;
    return true;
}

void LVGLBridge::refresh() {
    if (initialized_) {
        lv_timer_handler();
    }
}

void LVGLBridge::flushCallback(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    auto* driver = static_cast<hal::IDisplayDriver*>(lv_display_get_user_data(disp));

    if (driver) {
        hal::Rect rect{
            .x1 = area->x1,
            .y1 = area->y1,
            .x2 = area->x2,
            .y2 = area->y2
        };
        driver->flush(px_map, rect);
    }

    lv_display_flush_ready(disp);
}

}  // namespace oc::ui

#endif  // __has_include(<lvgl.h>)
