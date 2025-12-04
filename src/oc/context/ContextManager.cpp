#include "ContextManager.hpp"

#include <oc/core/event/Events.hpp>
#include <oc/core/event/IEventBus.hpp>

namespace oc::context {

ContextManager::ContextManager(ControlAPI& api, core::event::IEventBus& eventBus)
    : api_(api), eventBus_(eventBus) {}

ContextManager::~ContextManager() {
    if (active_) {
        active_->cleanup();
    }
    for (auto& pair : contexts_) {
        if (pair.second.get() != active_) {
            pair.second->cleanup();
        }
    }
}

bool ContextManager::switchTo(const std::string& id) {
    auto it = contexts_.find(id);
    if (it == contexts_.end()) {
        return false;
    }

    if (active_) {
        emitDeactivated(*active_);
        active_->onDisconnected();
    }

    active_ = it->second.get();
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
    eventBus_.emit(core::event::ContextRegisteredEvent(ctx.getId()));
}

void ContextManager::emitActivated(const IContext& ctx) {
    eventBus_.emit(core::event::ContextActivatedEvent(ctx.getId()));
}

void ContextManager::emitDeactivated(const IContext& ctx) {
    eventBus_.emit(core::event::ContextDeactivatedEvent(ctx.getId()));
}

void ContextManager::emitError(const IContext& ctx) {
    eventBus_.emit(core::event::ContextErrorEvent(ctx.getId()));
}

}  // namespace oc::context
