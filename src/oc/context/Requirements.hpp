#pragma once

#include <type_traits>

namespace oc::context {

/**
 * @brief Declares API requirements for a context
 *
 * Used with static constexpr REQUIRES member in IContext implementations.
 * The ContextManager validates these requirements at registration time.
 *
 * @code
 * class MyContext : public IContext {
 *     static constexpr Requirements REQUIRES {
 *         .button = true,
 *         .encoder = true,
 *         .midi = true
 *     };
 *     // ...
 * };
 * @endcode
 */
struct Requirements {
    bool button = false;
    bool encoder = false;
    bool midi = false;
    bool serial = false;
};

/**
 * @brief SFINAE helper to detect static REQUIRES member
 */
template <typename T, typename = void>
struct has_requirements : std::false_type {};

template <typename T>
struct has_requirements<T, std::void_t<decltype(T::REQUIRES)>> : std::true_type {};

}  // namespace oc::context
