#pragma once

#if __has_include(<lvgl.h>)

#include "IElement.hpp"

namespace oc::ui {

/**
 * @brief Interface for atomic UI widgets
 *
 * Represents simple UI elements that are typically always visible
 * once created (labels, progress bars, status indicators, etc.).
 *
 * Widgets don't have explicit show/hide semantics - their visibility
 * is managed by their parent container or view.
 *
 * Examples:
 * - Labels
 * - Progress bars
 * - Status indicators
 * - Arc displays
 */
class IWidget : public IElement {
public:
    // Inherits only getElement() from IElement
    // No additional methods - widgets are simple, always-visible elements
};

}  // namespace oc::ui

#endif  // __has_include(<lvgl.h>)
