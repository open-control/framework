#pragma once

/**
 * @file Traits.hpp
 * @brief SFINAE type traits for input binding system
 */

#include <type_traits>
#include <utility>

namespace oc::core::input {

/**
 * @brief SFINAE helper for duck-typed scope providers
 *
 * Detects if type T has a getIsActive() method returning something
 * convertible to oc::type::IsActiveFn. Used by ButtonBuilder, EncoderBuilder,
 * and ComboBuilder to extract activation predicates from scope providers.
 *
 * @code
 * struct MyScope {
 *     oc::type::IsActiveFn getIsActive() const { return [this]{ return active_; }; }
 * };
 *
 * if constexpr (has_getIsActive<MyScope>::value) {
 *     // MyScope has getIsActive()
 * }
 * @endcode
 */
template <typename T, typename = void>
struct has_getIsActive : std::false_type {};

template <typename T>
struct has_getIsActive<T, std::void_t<decltype(std::declval<const T&>().getIsActive())>>
    : std::true_type {};

/**
 * @brief Helper variable template for has_getIsActive
 */
template <typename T>
inline constexpr bool has_getIsActive_v = has_getIsActive<T>::value;

}  // namespace oc::core::input
