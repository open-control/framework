#pragma once

#if __has_include(<lvgl.h>)

#include "IElement.hpp"

namespace oc::ui {

/**
 * @brief Interface for full-screen views with system-managed lifecycle
 *
 * Views represent complete screens or pages. Unlike IComponent (which has
 * imperative show/hide control), views receive lifecycle notifications
 * (onActivate/onDeactivate) from external view management.
 *
 * Views are responsible for their content only.
 * External code (ViewManager, app logic) is responsible for transitions.
 *
 * Lifecycle model:
 * - IComponent: Imperative (code calls show/hide directly)
 * - IView: Declarative (system notifies onActivate/onDeactivate)
 *
 * Use getElement() to create scoped bindings that are only active
 * when this view is displayed.
 *
 * Examples:
 * - Main parameter view
 * - Settings screen
 * - Device selection view
 * - Splash screen
 */
class IView : public IElement {
public:
    /**
     * @brief Called when the view becomes active/visible
     *
     * Called by view manager when transitioning to this view.
     * The view should:
     * - Show its content (clear hidden flag on container)
     * - Start any animations or updates
     * - Set up input bindings (they auto-activate via lvglIsActive)
     */
    virtual void onActivate() = 0;

    /**
     * @brief Called when the view becomes inactive/hidden
     *
     * Called by view manager when transitioning away from this view.
     * The view should:
     * - Hide its content (set hidden flag on container)
     * - Stop animations or updates
     * - Save state if necessary
     * - Input bindings auto-deactivate via lvglIsActive
     */
    virtual void onDeactivate() = 0;

    /**
     * @brief Get unique view identifier
     * @return String identifier for logging/debug (e.g., "main", "settings")
     */
    virtual const char* getViewId() const = 0;
};

}  // namespace oc::ui

#endif  // __has_include(<lvgl.h>)
