#include "ContextManager.hpp"

#include <oc/api/ButtonAPI.hpp>
#include <oc/api/EncoderAPI.hpp>
#include <oc/api/MidiAPI.hpp>
#include <oc/core/event/Events.hpp>
#include <oc/core/event/IEventBus.hpp>
#include <oc/core/Warning.hpp>

namespace oc::context {

ContextManager::ContextManager(const APIs& apis) : apis_(apis) {}

ContextManager::~ContextManager() {
    if (active_) {
        active_->cleanup();
        active_.reset();
    }
}

bool ContextManager::begin() {
    if (default_id_ == INVALID_CONTEXT_ID) {
        return false;  // No contexts registered
    }
    return switchToImpl(default_id_);
}

bool ContextManager::switchToImpl(uint8_t id) {
    // Validate ID
    if (id >= MAX_CONTEXTS || factories_[id] == nullptr) {
        return false;  // Not registered
    }

    // Already active?
    if (active_id_ == id && active_) {
        return true;
    }

    // 1. Cleanup old context
    if (active_) {
        emitDeactivated(active_id_, *active_);
        active_->onDisconnected();
        active_->cleanup();

        // Clear bindings BEFORE destruction to prevent dangling callbacks
        if (apis_.button) {
            apis_.button->clearBindings();
        }
        if (apis_.encoder) {
            apis_.encoder->clearBindings();
        }

        // Stop MIDI notes to prevent hanging notes
        if (apis_.midi) {
            apis_.midi->allNotesOff();
        }

        active_.reset();  // Destroy old context
    }

    // 2. Create new context from factory
    active_ = factories_[id]();
    if (!active_) {
        emitError(id);
        active_id_ = INVALID_CONTEXT_ID;
        // Try fallback to default if this wasn't already the default
        if (id != default_id_ && default_id_ != INVALID_CONTEXT_ID) {
            return switchToImpl(default_id_);
        }
        core::warn("[ContextManager] CRITICAL: Default context failed to create - system has no active context");
        return false;
    }

    // 3. Initialize new context
    active_->setAPIs(apis_);
    if (!active_->initialize()) {
        emitError(id);
        active_.reset();
        active_id_ = INVALID_CONTEXT_ID;
        // Try fallback to default
        if (id != default_id_ && default_id_ != INVALID_CONTEXT_ID) {
            return switchToImpl(default_id_);
        }
        core::warn("[ContextManager] CRITICAL: Default context failed to initialize - system has no active context");
        return false;
    }

    // 4. Success
    active_id_ = id;
    active_->onConnected();
    emitActivated(id, *active_);

    return true;
}

void ContextManager::switchToDefault() {
    if (default_id_ != INVALID_CONTEXT_ID) {
        switchToImpl(default_id_);
    }
}

void ContextManager::update() {
    if (!active_) {
        return;
    }

    // Update the active context
    active_->update();

    // Check for disconnection (DAW contexts)
    if (!active_->isConnected()) {
        active_->onDisconnected();
        // Fall back to default context
        if (active_id_ != default_id_) {
            switchToDefault();
        }
    }
}

void ContextManager::emitActivated(uint8_t id, const IContext& ctx) {
    apis_.events.emit(core::event::ContextActivatedEvent(id, ctx.getName()));
}

void ContextManager::emitDeactivated(uint8_t id, const IContext& ctx) {
    apis_.events.emit(core::event::ContextDeactivatedEvent(id, ctx.getName()));
}

void ContextManager::emitError(uint8_t id) {
    apis_.events.emit(core::event::ContextErrorEvent(id));
}

}  // namespace oc::context
