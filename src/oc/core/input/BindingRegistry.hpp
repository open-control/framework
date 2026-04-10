#pragma once

/**
 * @file BindingRegistry.hpp
 * @brief Generic binding collection management
 *
 * Template class for managing collections of bindings (button or encoder).
 * Handles registration, removal, and scope-based clearing.
 */

#include <algorithm>
#include <vector>

#include <oc/core/input/Binding.hpp>
#include <oc/log/Log.hpp>
#include <oc/type/Ids.hpp>

namespace oc::core::input {

/**
 * @brief Generic registry for input bindings
 *
 * @tparam BindingType Type of binding (ButtonBinding or EncoderBinding)
 *
 * This is an internal helper class used by InputBinding.
 * It manages the collection of bindings and provides operations
 * for registration, removal, and scope-based clearing.
 */
template <typename BindingType>
class BindingRegistry {
public:
    /**
     * @brief Construct registry with maximum capacity
     * @param maxBindings Maximum number of bindings allowed
     * @param nextId Reference to shared ID counter
     */
    explicit BindingRegistry(size_t maxBindings, oc::type::BindingID& nextId)
        : max_bindings_(maxBindings), next_id_(nextId) {
        bindings_.reserve(maxBindings);
    }

    /**
     * @brief Register a new binding
     * @param binding The binding to register (id field will be set)
     * @return The assigned oc::type::BindingID, or 0 if at capacity
     */
    oc::type::BindingID add(BindingType binding) {
        if (bindings_.size() >= max_bindings_) {
            OC_LOG_ERROR("BindingRegistry: capacity reached used={} max={}",
                         bindings_.size(),
                         max_bindings_);
            return 0;
        }

        oc::type::BindingID id = next_id_++;
        if (id == 0) id = next_id_++;  // Skip 0 (invalid ID) on overflow
        binding.id = id;

        // Insert sorted by priority (higher priority first)
        auto it = std::find_if(bindings_.begin(), bindings_.end(),
                               [&](const BindingType& b) { return b.priority < binding.priority; });
        bindings_.insert(it, std::move(binding));
        return id;
    }

    /**
     * @brief Remove a binding by ID
     * @param id The binding ID to remove
     * @return true if found and removed
     */
    bool removeById(oc::type::BindingID id) {
        if (id == 0) return false;

        for (auto it = bindings_.begin(); it != bindings_.end(); ++it) {
            if (it->id == id) {
                bindings_.erase(it);
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Clear all bindings in a scope
     * @param scope The scope to clear
     */
    void clearScope(oc::type::ScopeID scope) {
        auto it = bindings_.begin();
        while (it != bindings_.end()) {
            if (it->scopeId == scope) {
                it = bindings_.erase(it);
            } else {
                ++it;
            }
        }
    }

    /**
     * @brief Clear all bindings
     */
    void clear() { bindings_.clear(); }

    /**
     * @brief Get all bindings (for iteration)
     */
    std::vector<BindingType>& bindings() { return bindings_; }
    const std::vector<BindingType>& bindings() const { return bindings_; }

    /**
     * @brief Get number of bindings
     */
    size_t size() const { return bindings_.size(); }

    /**
     * @brief Check if at capacity
     */
    bool isFull() const { return bindings_.size() >= max_bindings_; }

    /**
     * @brief Get registry capacity
     */
    size_t capacity() const { return max_bindings_; }

private:
    std::vector<BindingType> bindings_;
    size_t max_bindings_;
    oc::type::BindingID& next_id_;
};

}  // namespace oc::core::input
