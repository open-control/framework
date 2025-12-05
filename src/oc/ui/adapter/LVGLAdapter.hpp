#pragma once

#if __has_include(<lvgl.h>)
#include <lvgl.h>

#include <oc/core/struct/Binding.hpp>

namespace oc::ui {

/**
 * @brief Create IsActiveFn from LVGL object
 *
 * Returns a function that checks if the LVGL object exists and is not hidden.
 * Use this to create scoped bindings tied to view/component visibility.
 *
 * When the LVGL object is hidden (LV_OBJ_FLAG_HIDDEN), the binding
 * will be inactive and won't trigger callbacks.
 *
 * @param obj LVGL object to track (typically from IView::getElement())
 * @return IsActiveFn that returns true when obj is visible
 *
 * @code
 * // Binding only active when myView is visible
 * api.onPressed(
 *     ButtonID{1},
 *     oc::ui::lvglIsActive(myView.getElement()),
 *     []() { handlePress(); }
 * );
 * @endcode
 */
inline core::IsActiveFn lvglIsActive(lv_obj_t* obj) {
    return [obj]() {
        return obj && !lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN);
    };
}

/**
 * @brief Convert LVGL object pointer to ScopeID
 *
 * Creates a unique scope identifier from an LVGL object pointer.
 * Useful when you need to identify bindings by their scope.
 *
 * @param obj LVGL object
 * @return Unique ScopeID derived from pointer
 */
inline core::ScopeID lvglScopeID(lv_obj_t* obj) {
    return reinterpret_cast<core::ScopeID>(obj);
}

}  // namespace oc::ui

#endif  // __has_include(<lvgl.h>)
