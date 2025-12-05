#pragma once

#if __has_include(<lvgl.h>)
#include <lvgl.h>

#include <oc/hal/IDisplayDriver.hpp>

namespace oc::ui {

/**
 * @brief Configuration for LVGL bridge
 *
 * Consumer provides all buffers and driver.
 * Framework just wires LVGL display to our driver.
 */
struct LVGLBridgeConfig {
    uint16_t width;                 ///< Display width in pixels
    uint16_t height;                ///< Display height in pixels
    void* buffer1;                  ///< Primary draw buffer (DMAMEM on Teensy)
    void* buffer2 = nullptr;        ///< Optional second buffer for double-buffering
    size_t bufferSizeBytes;         ///< Size of each buffer in bytes
    hal::IDisplayDriver* driver;    ///< Display driver implementing flush

    /// Render mode (FULL recommended for small displays)
    lv_display_render_mode_t renderMode = LV_DISPLAY_RENDER_MODE_FULL;
};

/**
 * @brief Bridge between LVGL and Open Control display driver
 *
 * Connects LVGL rendering to any IDisplayDriver implementation.
 * Consumer is responsible for LVGL initialization and memory configuration.
 *
 * Prerequisites (consumer responsibility):
 * - Configure lv_conf.h (memory, features, tick)
 * - Call lv_init() before bridge.init()
 * - Provide draw buffer(s) in config
 *
 * @code
 * // Consumer setup
 * DMAMEM lv_color_t buf[320 * 240];
 *
 * lv_init();  // Consumer calls this
 *
 * oc::ui::LVGLBridgeConfig cfg{
 *     .width = 320,
 *     .height = 240,
 *     .buffer1 = buf,
 *     .bufferSizeBytes = sizeof(buf),
 *     .driver = &myDisplayDriver
 * };
 *
 * oc::ui::LVGLBridge bridge(cfg);
 * bridge.init();
 *
 * // Main loop
 * bridge.refresh();
 * @endcode
 */
class LVGLBridge {
public:
    explicit LVGLBridge(const LVGLBridgeConfig& config);
    ~LVGLBridge();

    // Non-copyable
    LVGLBridge(const LVGLBridge&) = delete;
    LVGLBridge& operator=(const LVGLBridge&) = delete;

    /**
     * @brief Initialize LVGL display
     *
     * Creates lv_display, sets buffers, wires flush callback.
     * Requires lv_init() to have been called first by consumer.
     *
     * @return true if successful
     */
    bool init();

    /**
     * @brief Process LVGL timers and rendering
     *
     * Call this regularly in main loop (typically every frame).
     * Triggers rendering and flush to display driver.
     */
    void refresh();

    /**
     * @brief Check if bridge is initialized
     */
    bool isInitialized() const { return initialized_; }

    /**
     * @brief Get underlying LVGL display
     * @return Display pointer or nullptr if not initialized
     */
    lv_display_t* getDisplay() const { return display_; }

private:
    static void flushCallback(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map);

    LVGLBridgeConfig config_;
    lv_display_t* display_ = nullptr;
    bool initialized_ = false;
};

}  // namespace oc::ui

#endif  // __has_include(<lvgl.h>)
