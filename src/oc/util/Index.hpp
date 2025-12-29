#pragma once

/**
 * @file Index.hpp
 * @brief Index manipulation utilities for lists and navigation
 */

#include <cstddef>
#include <cstdint>

namespace oc::util {

/**
 * @brief Wrap index within range [0, modulo) with proper negative handling
 *
 * Unlike standard modulo, this correctly handles negative values:
 * - wrapIndex(-1, 5) returns 4 (not -1)
 * - wrapIndex(5, 5) returns 0
 * - wrapIndex(3, 5) returns 3
 *
 * @param value Index to wrap (can be negative)
 * @param modulo Range size (must be > 0)
 * @return Wrapped index in range [0, modulo)
 */
inline int wrapIndex(int value, int modulo) {
    return ((value % modulo) + modulo) % modulo;
}

/**
 * @brief Check if prefetch should be triggered for windowed loading
 *
 * Used with paginated/windowed data loading patterns where data is fetched
 * in chunks. Triggers prefetch when the current position approaches the
 * boundary of already-loaded data.
 *
 * @tparam Threshold Number of items before boundary to trigger prefetch (default: 3)
 * @param currentIndex Current navigation position
 * @param loadedUpTo Highest index that has been loaded
 * @param totalCount Total number of items available
 * @return true if next data window should be prefetched
 *
 * @code
 * void navigate(int delta) {
 *     int newIndex = wrapIndex(currentIndex + delta, totalCount);
 *     selector.currentIndex.set(newIndex);
 *
 *     if (shouldPrefetch(newIndex, selector.loadedUpTo.get(), totalCount)) {
 *         protocol.send(RequestNextWindow{selector.loadedUpTo.get()});
 *     }
 * }
 * @endcode
 */
template <size_t Threshold = 3>
inline bool shouldPrefetch(size_t currentIndex, size_t loadedUpTo, size_t totalCount) {
    return loadedUpTo > Threshold
        && currentIndex >= loadedUpTo - Threshold
        && loadedUpTo < totalCount;
}

}  // namespace oc::util
