#pragma once

#if __has_include(<lvgl.h>)

#include "IWidget.hpp"

namespace oc::ui {

/**
 * @brief Interface for UI components with explicit visibility control
 *
 * Represents UI elements that can be shown/hidden imperatively by code
 * (overlays, popups, modals, selectors, etc.).
 *
 * Components manage their own visibility state and can be toggled
 * independently from their parent view.
 *
 * Examples:
 * - Modal dialogs
 * - Dropdown selectors
 * - Popup overlays
 * - Context menus
 * - Tooltips
 *
 * Note: Views (IView) do NOT inherit from IComponent, as their
 * lifecycle is managed externally (onActivate/onDeactivate),
 * not by direct show/hide calls.
 */
class IComponent : public IWidget {
public:
    /**
     * @brief Show the component
     *
     * Makes the component visible. Implementation typically:
     * - Clears LVGL hidden flag
     * - Triggers show animations
     * - Updates internal visibility state
     */
    virtual void show() = 0;

    /**
     * @brief Hide the component
     *
     * Makes the component invisible. Implementation typically:
     * - Sets LVGL hidden flag
     * - Stops animations
     * - Updates internal visibility state
     */
    virtual void hide() = 0;

    /**
     * @brief Check if component is currently visible
     * @return true if visible, false otherwise
     */
    virtual bool isVisible() const = 0;
};

}  // namespace oc::ui

#endif  // __has_include(<lvgl.h>)
