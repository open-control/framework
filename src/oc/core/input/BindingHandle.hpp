#pragma once

#include <cstdint>

namespace oc::core {

// Forward declaration
namespace input {
class InputBinding;
}

/**
 * @brief Unique identifier for a registered binding
 *
 * Used internally to track and remove bindings.
 * Value 0 indicates an invalid/unregistered binding.
 */
using BindingID = uint32_t;

/**
 * @brief Handle returned by fluent API for optional unbinding
 *
 * Allows the caller to later unbind a registered binding.
 * The handle is lightweight (two pointers) and can be stored
 * or discarded if unbinding is not needed.
 *
 * Usage:
 * @code
 * auto handle = api.button(BTN_1).onPress().then(callback);
 *
 * // Later, if needed:
 * handle.unbind();
 * @endcode
 */
class BindingHandle {
public:
    /**
     * @brief Default constructor creates an invalid handle
     */
    BindingHandle() = default;

    /**
     * @brief Construct a valid handle
     *
     * @param registry Pointer to InputBinding that owns this binding
     * @param id Unique ID of the binding
     */
    BindingHandle(input::InputBinding* registry, BindingID id)
        : registry_(registry), id_(id) {}

    /**
     * @brief Remove the binding from the registry
     *
     * After calling unbind(), isValid() returns false.
     * Safe to call multiple times (subsequent calls are no-ops).
     */
    void unbind();

    /**
     * @brief Check if this handle refers to a valid binding
     *
     * @return true if the binding exists and can be unbound
     */
    bool isValid() const { return registry_ != nullptr && id_ != 0; }

    /**
     * @brief Get the binding ID
     *
     * @return The internal binding ID, or 0 if invalid
     */
    BindingID id() const { return id_; }

    /**
     * @brief Create an invalid handle
     *
     * @return A handle that refers to no binding
     */
    static BindingHandle invalid() { return BindingHandle(); }

private:
    input::InputBinding* registry_ = nullptr;
    BindingID id_ = 0;
};

}  // namespace oc::core
