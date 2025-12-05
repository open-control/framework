#include "ContextManager.hpp"

#include <oc/api/ControlAPI.hpp>
#include <oc/core/event/Events.hpp>
#include <oc/core/event/IEventBus.hpp>

namespace oc::context {

ContextManager::ContextManager(oc::api::ControlAPI& api) : api_(api) {}

ContextManager::~ContextManager() {
    if (active_) {
        active_->cleanup();
    }
    // Only cleanup contexts that were initialized
    for (auto& pair : contexts_) {
        if (pair.second.get() != active_ && initialized_ids_.count(pair.first)) {
            pair.second->cleanup();
        }
    }
}

bool ContextManager::switchTo(const std::string& id) {
    auto it = contexts_.find(id);
    if (it == contexts_.end()) {
        return false;
    }

    IContext* ctx = it->second.get();

    // Lazy initialization: initialize on first switch
    if (initialized_ids_.count(id) == 0) {
        if (!ctx->initialize(api_)) {
            emitError(*ctx);
            return false;
        }
        initialized_ids_.insert(id);
    }

    // Deactivate previous context
    if (active_) {
        emitDeactivated(*active_);
        active_->onDisconnected();
    }

    // Activate new context
    active_ = ctx;
    active_->onConnected();
    emitActivated(*active_);

    return true;
}

void ContextManager::switchToDefault() {
    if (!default_id_.empty()) {
        switchTo(default_id_);
    }
}

void ContextManager::setDefault(const std::string& id) {
    default_id_ = id;
}

bool ContextManager::hasContext(const std::string& id) const {
    return contexts_.find(id) != contexts_.end();
}

void ContextManager::update() {
    if (active_) {
        active_->update();
    }
}

void ContextManager::emitRegistered(const IContext& ctx) {
    api_.eventBus().emit(core::event::ContextRegisteredEvent(ctx.getId()));
}

void ContextManager::emitActivated(const IContext& ctx) {
    api_.eventBus().emit(core::event::ContextActivatedEvent(ctx.getId()));
}

void ContextManager::emitDeactivated(const IContext& ctx) {
    api_.eventBus().emit(core::event::ContextDeactivatedEvent(ctx.getId()));
}

void ContextManager::emitError(const IContext& ctx) {
    api_.eventBus().emit(core::event::ContextErrorEvent(ctx.getId()));
}

}  // namespace oc::context
