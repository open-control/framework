#pragma once

/**
 * @file IContextWithAPIs.hpp
 * @brief Optional interface for contexts that receive injected APIs (Level 3)
 *
 * Contexts that need access to ButtonAPI / EncoderAPI / MidiAPI should either:
 * - inherit from oc::context::ContextBase (recommended), or
 * - implement this interface directly.
 *
 * This keeps oc::interface::IContext free of higher-level dependencies.
 */

namespace oc::context {

struct APIs;

/**
 * @brief Optional interface for API injection into a context
 *
 * Called by ContextManager before init().
 */
class IContextWithAPIs {
public:
    virtual ~IContextWithAPIs() = default;

    virtual void setAPIs(const APIs& apis) = 0;
};

}  // namespace oc::context
