#pragma once

#if __has_include(<lvgl.h>)
#include <lvgl.h>

namespace oc::ui {

/**
 * @brief Base interface for UI elements backed by LVGL objects
 *
 * Provides access to the underlying LVGL object for:
 * - Scoped input bindings (via LVGLAdapter)
 * - Direct LVGL manipulation when needed
 * - Parent-child relationships
 *
 * All UI elements (widgets, components, views) inherit this interface.
 */
class IElement {
public:
    virtual ~IElement() = default;

    /**
     * @brief Get the underlying LVGL object
     * @return LVGL object pointer, or nullptr if not created/destroyed
     *
     * This object can be used with LVGLAdapter to create scoped bindings
     * that are only active when this element is visible.
     *
     * For composite elements, this should return the top-level container.
     */
    virtual lv_obj_t* getElement() const = 0;
};

}  // namespace oc::ui

#endif  // __has_include(<lvgl.h>)
