#include "ContextManager.hpp"

#include <oc/api/ButtonAPI.hpp>
#include <oc/api/EncoderAPI.hpp>
#include <oc/api/MidiAPI.hpp>
#include <oc/core/event/Events.hpp>
#include <oc/interface/IEventBus.hpp>
#include <oc/core/Warning.hpp>

namespace oc::context {

ContextManager::ContextManager(const APIs& apis) : apis_(apis) {}

ContextManager::~ContextManager() {
    if (active_) {
        active_->cleanup();
        active_.reset();
    }
}

core::Result<void> ContextManager::begin() {
    using R = core::Result<void>;
    using E = core::ErrorCode;

    if (default_id_ == INVALID_CONTEXT_ID) {
        return R::err({E::CONTEXT_NOT_REGISTERED, "no default context"});
    }
    if (!switchToImpl(default_id_)) {
        return R::err({E::CONTEXT_INIT_FAILED, "default context"});
    }
    return R::ok();
}

bool ContextManager::switchToImpl(uint8_t id) {
    if (id >= MAX_CONTEXTS || factories_[id] == nullptr) {
        return false;
    }

    if (active_id_ == id && active_) {
        return true;
    }

    if (active_) {
        emitDeactivated(active_id_, *active_);
        active_->onDisconnected();
        active_->cleanup();

        if (apis_.button) {
            apis_.button->clearBindings();
        }
        if (apis_.encoder) {
            apis_.encoder->clearBindings();
        }
        if (apis_.midi) {
            apis_.midi->allNotesOff();
        }

        active_.reset();
    }

    active_ = factories_[id]();
    if (!active_) {
        emitError(id);
        active_id_ = INVALID_CONTEXT_ID;
        if (id != default_id_ && default_id_ != INVALID_CONTEXT_ID) {
            return switchToImpl(default_id_);
        }
        core::warn("[ContextManager] CRITICAL: Default context failed to create");
        return false;
    }

    active_->setAPIs(apis_);
    if (!active_->initialize()) {
        emitError(id);
        active_.reset();
        active_id_ = INVALID_CONTEXT_ID;
        if (id != default_id_ && default_id_ != INVALID_CONTEXT_ID) {
            return switchToImpl(default_id_);
        }
        core::warn("[ContextManager] CRITICAL: Default context failed to initialize");
        return false;
    }

    active_id_ = id;
    active_->onConnected();
    emitActivated(id, *active_);

    return true;
}

void ContextManager::processPendingSwitch() {
    if (pending_switch_) {
        uint8_t targetId = *pending_switch_;
        pending_switch_.reset();
        switchToImpl(targetId);
    }
}

void ContextManager::update() {
    if (!active_) {
        return;
    }

    active_->update();

    // Process deferred switch AFTER update returns (safe lifecycle)
    processPendingSwitch();

    // Check for disconnection (DAW contexts) - schedule deferred switch
    if (active_ && !active_->isConnected()) {
        active_->onDisconnected();
        if (active_id_ != default_id_ && default_id_ != INVALID_CONTEXT_ID) {
            pending_switch_ = default_id_;
        }
    }
}

void ContextManager::emitActivated(uint8_t id, const interface::IContext& ctx) {
    apis_.events.emit(core::event::ContextActivatedEvent(id, ctx.getName()));
}

void ContextManager::emitDeactivated(uint8_t id, const interface::IContext& ctx) {
    apis_.events.emit(core::event::ContextDeactivatedEvent(id, ctx.getName()));
}

void ContextManager::emitError(uint8_t id) {
    apis_.events.emit(core::event::ContextErrorEvent(id));
}

}  // namespace oc::context
