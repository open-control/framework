#include "InputBinding.hpp"

#include <config/PlatformCompat.hpp>
#include <oc/core/event/Events.hpp>
#include <oc/log/Log.hpp>
#include <utility>

namespace oc::core::input {

using namespace event;

// ═══════════════════════════════════════════════════════════════════════════
// Construction / Destruction
// ═══════════════════════════════════════════════════════════════════════════

FLASHMEM InputBinding::InputBinding(interface::IEventBus& eventBus,
                                    oc::type::TimeProvider timeProvider,
                                    const InputConfig& config)
    : button_registry_(MAX_BUTTON_BINDINGS, next_binding_id_),
      encoder_registry_(MAX_ENCODER_BINDINGS, next_binding_id_),
      gesture_(config),
      event_bus_(eventBus),
      time_provider_(timeProvider),
      config_(config) {
    if (!time_provider_) {
        OC_LOG_WARN("{}", "[InputBinding] No oc::type::TimeProvider - long press and double tap detection disabled");
    }

    encoder_sub_ = event_bus_.on(oc::type::EventCategory::USER_INPUT, InputEvent::ENCODER_CHANGED,
                                 [this](const oc::type::Event& e) { onEncoderChanged(e); });

    button_press_sub_ = event_bus_.on(oc::type::EventCategory::USER_INPUT, InputEvent::BUTTON_PRESS,
                                      [this](const oc::type::Event& e) { onButtonPress(e); });

    button_release_sub_ = event_bus_.on(oc::type::EventCategory::USER_INPUT, InputEvent::BUTTON_RELEASE,
                                        [this](const oc::type::Event& e) { onButtonRelease(e); });
}

FLASHMEM InputBinding::~InputBinding() {
    event_bus_.off(encoder_sub_);
    event_bus_.off(button_press_sub_);
    event_bus_.off(button_release_sub_);
}

// ═══════════════════════════════════════════════════════════════════════════
// Public API - Scope Management
// ═══════════════════════════════════════════════════════════════════════════

FLASHMEM void InputBinding::clearScope(oc::type::ScopeID scope) {
    button_registry_.clearScope(scope);
    encoder_registry_.clearScope(scope);
    gesture_routes_.clearForScope(scope);
    latch_.releaseForScope(scope);
}

FLASHMEM void InputBinding::clearBindings() {
    button_registry_.clear();
    encoder_registry_.clear();
    gesture_routes_.reset();
    latch_.reset();
    gesture_.reset();
}

FLASHMEM void InputBinding::clearButtonBindings() {
    button_registry_.clear();
    gesture_routes_.reset();
    latch_.reset();
    gesture_.reset();
}

FLASHMEM void InputBinding::clearEncoderBindings() {
    encoder_registry_.clear();
}

FLASHMEM void InputBinding::clearButtonScope(oc::type::ScopeID scope) {
    button_registry_.clearScope(scope);
    gesture_routes_.clearForScope(scope);
    latch_.releaseForScope(scope);
}

FLASHMEM void InputBinding::clearEncoderScope(oc::type::ScopeID scope) {
    encoder_registry_.clearScope(scope);
}

// ═══════════════════════════════════════════════════════════════════════════
// Public API - Latch
// ═══════════════════════════════════════════════════════════════════════════

bool InputBinding::isLatched(oc::type::ButtonID btn) const {
    return latch_.isLatched(btn);
}

void InputBinding::clearLatch(oc::type::ButtonID btn) {
    latch_.release(btn);
}

void InputBinding::clearLatchesForScope(oc::type::ScopeID scope) {
    latch_.releaseForScope(scope);
}

// ═══════════════════════════════════════════════════════════════════════════
// Public API - State
// ═══════════════════════════════════════════════════════════════════════════

bool InputBinding::isButtonPressed(oc::type::ButtonID id) const {
    return gesture_.isPressed(id);
}

FLASHMEM void InputBinding::handoffPress(oc::type::ButtonID id,
                                         oc::type::ScopeID scope) {
    if (id >= MAX_BUTTONS) return;
    auto& route = gesture_routes_.route(id);

    if (config_.gestureRoutingPolicy == GestureRoutingPolicy::Legacy) {
        route.ownerScope = scope;
        return;
    }

    if (!route.active || !gesture_.isPressed(id)) return;

    const auto origin = route.originScope;
    captureRouteForScope(id, scope, false, true);
    auto& handedRoute = gesture_routes_.route(id);
    handedRoute.originScope = origin;
    handedRoute.explicitHandoff = true;
    diagnostics_.routeHandoffs++;
    trace({
        .stage = InputBindingTraceStage::RouteHandoff,
        .domain = InputBindingTraceDomain::Button,
        .buttonId = id,
        .scopeId = scope,
        .authorityScope = currentAuthority(),
        .scoped = scope != 0,
        .active = true,
    });
}

FLASHMEM void InputBinding::consumePress(oc::type::ButtonID id) {
    if (id >= MAX_BUTTONS) return;
    auto& route = gesture_routes_.route(id);
    if (!route.active || route.consumed) return;
    route.consumed = true;
    diagnostics_.consumedGestures++;
}

FLASHMEM void InputBinding::quarantinePressedButtons() {
    if (config_.gestureRoutingPolicy != GestureRoutingPolicy::PressScoped) return;

    for (size_t index = 0; index < MAX_BUTTONS; ++index) {
        const auto button = static_cast<oc::type::ButtonID>(index);
        if (!gesture_.isPressed(button)) continue;
        auto& route = gesture_routes_.route(button);
        if (!route.active || route.consumed) continue;
        route.consumed = true;
        diagnostics_.quarantinedGestures++;
        diagnostics_.consumedGestures++;
        traceConsumed(button, ButtonBindingType::RELEASE);
    }
}

FLASHMEM void InputBinding::setBindingsEnabled(bool enabled) {
    bindings_enabled_ = enabled;
}

FLASHMEM void InputBinding::setTraceCallback(InputBindingTraceCallback callback) {
    trace_callback_ = std::move(callback);
}

FLASHMEM void InputBinding::setAuthorityResolver(const AuthorityResolver* resolver) {
    authority_resolver_ = resolver;
    // Invalidate any outstanding tokens
    authority_token_++;
    if (authority_token_ == 0) authority_token_ = 1;
}

FLASHMEM InputBinding::AuthorityToken InputBinding::setAuthorityResolverScoped(const AuthorityResolver* resolver) {
    authority_resolver_ = resolver;
    authority_token_++;
    if (authority_token_ == 0) authority_token_ = 1;
    return authority_token_;
}

FLASHMEM void InputBinding::clearAuthorityResolver(AuthorityToken token) {
    if (token != 0 && token == authority_token_) {
        authority_resolver_ = nullptr;
    }
}

void InputBinding::processTick() {
    if (time_provider_) {
        current_time_ = time_provider_();
    }
    for (size_t i = 0; i < MAX_BUTTONS; ++i) {
        if (gesture_.isPressed(static_cast<oc::type::ButtonID>(i))) {
            checkLongPress(static_cast<oc::type::ButtonID>(i), current_time_);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Public API - Registration
// ═══════════════════════════════════════════════════════════════════════════

FLASHMEM oc::type::BindingID InputBinding::registerButtonBinding(ButtonBinding binding) {
    return button_registry_.add(std::move(binding));
}

FLASHMEM oc::type::BindingID InputBinding::registerEncoderBinding(EncoderBinding binding) {
    return encoder_registry_.add(std::move(binding));
}

FLASHMEM bool InputBinding::removeById(oc::type::BindingID id) {
    if (id == 0) return false;
    if (button_registry_.removeById(id)) return true;
    return encoder_registry_.removeById(id);
}

// ═══════════════════════════════════════════════════════════════════════════
// oc::type::Event Handlers
// ═══════════════════════════════════════════════════════════════════════════

void InputBinding::onEncoderChanged(const oc::type::Event& event) {
    auto& evt = static_cast<const EncoderChangedEvent&>(event);
    trace({
        .stage = InputBindingTraceStage::Event,
        .domain = InputBindingTraceDomain::Encoder,
        .encoderId = evt.encoderId,
        .authorityScope = currentAuthority(),
        .encoderValue = evt.normalizedValue,
    });
    dispatchEncoderEvent(evt.encoderId, evt.normalizedValue);
}

void InputBinding::onButtonPress(const oc::type::Event& event) {
    auto& evt = static_cast<const ButtonPressEvent&>(event);
    oc::type::ButtonID id = evt.buttonId;
    if (id >= MAX_BUTTONS) return;

    trace({
        .stage = InputBindingTraceStage::Event,
        .domain = InputBindingTraceDomain::Button,
        .buttonId = id,
        .buttonType = ButtonBindingType::PRESS,
        .authorityScope = currentAuthority(),
    });

    gesture_.onButtonPress(id, current_time_);

    if (config_.gestureRoutingPolicy == GestureRoutingPolicy::PressScoped) {
        beginPressScopedGesture(id);
        return;
    }

    // Dispatch to scopes, excluding the one that owns the latch (if any)
    oc::type::ScopeID newOwner = dispatchPress(id, latch_.owner(id));

    // Allow press handlers to transfer ownership (e.g. when opening a stacked
    // overlay on press and routing the paired release to that new scope).
    auto& route = gesture_routes_.route(id);
    const oc::type::ScopeID overriddenOwner = route.ownerScope;
    if (overriddenOwner == 0 || overriddenOwner == newOwner) {
        route.ownerScope = newOwner;
    }
    route.active = newOwner != 0;
}

void InputBinding::onButtonRelease(const oc::type::Event& event) {
    auto& evt = static_cast<const ButtonReleaseEvent&>(event);
    oc::type::ButtonID id = evt.buttonId;
    if (id >= MAX_BUTTONS) return;

    trace({
        .stage = InputBindingTraceStage::Event,
        .domain = InputBindingTraceDomain::Button,
        .buttonId = id,
        .buttonType = ButtonBindingType::RELEASE,
        .authorityScope = currentAuthority(),
    });

    const uint32_t now = current_time_;
    const uint32_t pressDuration = now - gesture_.pressTime(id);
    const oc::type::ScopeID latchOwner = latch_.owner(id);
    const oc::type::ScopeID pressOwner = gesture_routes_.route(id).ownerScope;

    // Combo detection (only if not latched and using legacy resolution)
    if (config_.gestureRoutingPolicy == GestureRoutingPolicy::Legacy && latchOwner == 0) {
        checkCombo(id);
    }

    if (config_.gestureRoutingPolicy == GestureRoutingPolicy::PressScoped) {
        const auto& route = gesture_routes_.route(id);
        if (route.active && !route.consumed && route.comboBinding != 0) {
            dispatchExactButtonBinding(id,
                                       ButtonBindingType::COMBO,
                                       route.comboBinding,
                                       true);
        }
    }

    gesture_.onButtonRelease(id, now);

    if (config_.gestureRoutingPolicy == GestureRoutingPolicy::PressScoped) {
        releasePressScopedGesture(id, pressDuration);
        return;
    }

    // Handle release based on ownership state
    if (pressOwner != 0) {
        handleScopedRelease(id, pressOwner, pressDuration);
    } else if (latchOwner != 0) {
        handleLatchedRelease(id, latchOwner);
    } else {
        dispatchButtonEvent(id, ButtonBindingType::RELEASE);
    }

    gesture_routes_.clear(id);
    checkDoubleTap(id, now);
}

// ═══════════════════════════════════════════════════════════════════════════
// Strict Physical-Gesture Routing
// ═══════════════════════════════════════════════════════════════════════════

FLASHMEM void InputBinding::beginPressScopedGesture(oc::type::ButtonID id) {
    gesture_routes_.clear(id);

    const auto latchOwner = latch_.owner(id);
    if (latchOwner != 0) {
        captureRouteForScope(id, latchOwner, false, false);
        gesture_routes_.route(id).pressBinding = 0;
        return;
    }

    if (hasActiveGlobalPassThrough(id)) {
        captureRouteForScope(id, 0, true, false);
    } else {
        const auto authority = currentAuthority();
        if (authority != 0) {
            captureRouteForScope(id, authority, false, false);
        } else if (config_.globalRoutingPolicy == GlobalRoutingPolicy::Fallback) {
            captureRouteForScope(id, 0, false, false);
        } else {
            auto& route = gesture_routes_.route(id);
            route.active = true;
            route.consumed = true;
            diagnostics_.consumedGestures++;
            traceConsumed(id, ButtonBindingType::PRESS);
            return;
        }
    }

    auto& route = gesture_routes_.route(id);
    if (!route.active || route.consumed || route.pressBinding == 0) return;

    const auto pressBinding = route.pressBinding;
    dispatchExactButtonBinding(id, ButtonBindingType::PRESS, pressBinding, true);

    // A press callback may deliberately transfer the gesture after changing
    // authority. Never overwrite that explicit route.
    auto& routedAfterPress = gesture_routes_.route(id);
    if (!routedAfterPress.active || routedAfterPress.explicitHandoff) return;

    // Same-scope press callbacks may change an inline mode. Capture release,
    // long-press and double-tap predicates after that state transition so the
    // route represents the state entered by the press.
    const bool quarantined = routedAfterPress.consumed;
    const auto owner = routedAfterPress.ownerScope;
    const bool passThrough = routedAfterPress.globalPassThrough;

    const auto selection = selectButtonRoute(
        id,
        owner,
        passThrough,
        owner != 0,
        false
    );

    routedAfterPress.releaseBinding = selection.release.id;
    routedAfterPress.longPressBinding = selection.longPress.id;
    routedAfterPress.doubleTapBinding = selection.doubleTap.id;
    routedAfterPress.comboBinding = selection.combo.id;
    routedAfterPress.consumed = quarantined || selection.ambiguous();
    if (!quarantined && routedAfterPress.consumed) {
        diagnostics_.consumedGestures++;
        traceConsumed(id, ButtonBindingType::PRESS);
    }
}

FLASHMEM void InputBinding::releasePressScopedGesture(
    oc::type::ButtonID id,
    uint32_t pressDuration
) {
    auto& route = gesture_routes_.route(id);
    const auto latchOwner = latch_.owner(id);

    if (!route.active || route.consumed) {
        traceConsumed(id, ButtonBindingType::RELEASE);
        if (latchOwner != 0) latch_.release(id);
        gesture_routes_.clear(id);
        return;
    }

    if (latchOwner != 0) {
        if (route.releaseBinding != 0) {
            dispatchExactButtonBinding(id,
                                       ButtonBindingType::RELEASE,
                                       route.releaseBinding,
                                       true);
        } else {
            traceConsumed(id, ButtonBindingType::RELEASE);
        }
        latch_.release(id);
    } else if (shouldActivateLatch(id, route.ownerScope, pressDuration)) {
        latch_.activate(id, route.ownerScope);
    } else if (route.releaseBinding != 0) {
        dispatchExactButtonBinding(id,
                                   ButtonBindingType::RELEASE,
                                   route.releaseBinding,
                                   true);
    } else {
        traceConsumed(id, ButtonBindingType::RELEASE);
    }

    if (route.doubleTapBinding != 0 && gesture_.tapCount(id) >= 2) {
        auto* binding = findButtonBinding(route.doubleTapBinding);
        if (binding &&
            gesture_.checkDoubleTap(id, current_time_, binding->doubleTapWindowMs) &&
            dispatchExactButtonBinding(id,
                                       ButtonBindingType::DOUBLE_TAP,
                                       route.doubleTapBinding,
                                       true)) {
            gesture_.resetTapCount(id);
        }
    }

    gesture_routes_.clear(id);
}

FLASHMEM void InputBinding::captureRouteForScope(
    oc::type::ButtonID id,
    oc::type::ScopeID scope,
    bool globalPassThrough,
    bool explicitHandoff
) {
    auto& route = gesture_routes_.route(id);
    route = {};
    route.active = true;
    route.explicitHandoff = explicitHandoff;
    route.globalPassThrough = globalPassThrough;
    route.originScope = currentAuthority();
    route.ownerScope = scope;

    const bool enforceAuthority = scope != 0 && !explicitHandoff;
    const auto selection = selectButtonRoute(
        id,
        scope,
        globalPassThrough,
        enforceAuthority,
        true
    );

    route.pressBinding = selection.press.id;
    route.releaseBinding = selection.release.id;
    route.longPressBinding = selection.longPress.id;
    route.doubleTapBinding = selection.doubleTap.id;
    route.comboBinding = selection.combo.id;
    route.consumed = selection.ambiguous();

    const bool hasRoute =
        route.pressBinding != 0 || route.releaseBinding != 0 ||
        route.longPressBinding != 0 || route.doubleTapBinding != 0 ||
        route.comboBinding != 0;
    if (!hasRoute) route.consumed = true;

    diagnostics_.routeCaptures++;
    if (route.consumed) diagnostics_.consumedGestures++;
    trace({
        .stage = InputBindingTraceStage::RouteCapture,
        .domain = InputBindingTraceDomain::Button,
        .buttonId = id,
        .bindingId = route.pressBinding != 0 ? route.pressBinding : route.releaseBinding,
        .scopeId = scope,
        .authorityScope = currentAuthority(),
        .scoped = scope != 0,
        .active = hasRoute,
        .dispatched = false,
    });
    if (route.consumed) traceConsumed(id, ButtonBindingType::PRESS);
}

FLASHMEM InputBinding::ButtonRouteSelection InputBinding::selectButtonRoute(
    oc::type::ButtonID id,
    oc::type::ScopeID scope,
    bool globalPassThroughOnly,
    bool enforceAuthority,
    bool includePress
) {
    ButtonRouteSelection result{};

    const auto selectionFor = [&result](
        ButtonBindingType type
    ) -> BindingSelection* {
        switch (type) {
            case ButtonBindingType::PRESS:
                return &result.press;
            case ButtonBindingType::RELEASE:
                return &result.release;
            case ButtonBindingType::LONG_PRESS:
                return &result.longPress;
            case ButtonBindingType::DOUBLE_TAP:
                return &result.doubleTap;
            case ButtonBindingType::COMBO:
                return &result.combo;
        }
        return nullptr;
    };

    for (auto& binding : button_registry_.bindings()) {
        if (!binding.enabled || binding.scopeId != scope ||
            (!includePress &&
             binding.type == ButtonBindingType::PRESS)) {
            continue;
        }

        const bool buttonMatches =
            binding.type == ButtonBindingType::COMBO
                ? binding.buttonId == id ||
                      (binding.secondaryButton.has_value() &&
                       *binding.secondaryButton == id)
                : binding.buttonId == id;
        if (!buttonMatches) continue;
        if (globalPassThroughOnly && !binding.globalPassThrough) continue;

        const bool active = isBindingActive(binding);
        const bool authority = !enforceAuthority || hasAuthority(binding.scopeId);
        trace({
            .stage = InputBindingTraceStage::Candidate,
            .domain = InputBindingTraceDomain::Button,
            .buttonId = id,
            .buttonType = binding.type,
            .bindingId = binding.id,
            .scopeId = binding.scopeId,
            .authorityScope = currentAuthority(),
            .scoped = binding.scopeId != 0,
            .active = active,
            .authority = authority,
        });
        if (!active || !authority || !binding.action) continue;

        auto* selected = selectionFor(binding.type);
        if (selected == nullptr) continue;
        if (selected->candidateCount == 0U ||
            binding.priority > selected->priority) {
            selected->id = binding.id;
            selected->candidateCount = 1U;
            selected->priority = binding.priority;
            continue;
        }
        if (binding.priority == selected->priority &&
            selected->candidateCount != UINT8_MAX) {
            selected->candidateCount++;
        }
    }

    if (includePress) {
        finalizeButtonSelection(
            result.press,
            ButtonBindingType::PRESS,
            id,
            scope
        );
    }
    finalizeButtonSelection(
        result.release,
        ButtonBindingType::RELEASE,
        id,
        scope
    );
    finalizeButtonSelection(
        result.longPress,
        ButtonBindingType::LONG_PRESS,
        id,
        scope
    );
    finalizeButtonSelection(
        result.doubleTap,
        ButtonBindingType::DOUBLE_TAP,
        id,
        scope
    );
    finalizeButtonSelection(
        result.combo,
        ButtonBindingType::COMBO,
        id,
        scope
    );

    return result;
}

FLASHMEM void InputBinding::finalizeButtonSelection(
    BindingSelection& selection,
    ButtonBindingType type,
    oc::type::ButtonID id,
    oc::type::ScopeID scope
) {
    if (selection.candidateCount <= 1U ||
        config_.ambiguityPolicy != BindingAmbiguityPolicy::FailClosed) {
        return;
    }

    selection.id = 0;
    selection.ambiguous = true;
    diagnostics_.ambiguities++;
    OC_LOG_ERROR(
        "[InputBinding] ambiguous route button={} gesture={} scope={} "
        "authority={} candidates={} priority={}; gesture consumed",
        static_cast<unsigned>(id),
        static_cast<unsigned>(type),
        static_cast<unsigned>(scope),
        static_cast<unsigned>(currentAuthority()),
        static_cast<unsigned>(selection.candidateCount),
        static_cast<int>(selection.priority)
    );
    trace({
        .stage = InputBindingTraceStage::Ambiguous,
        .domain = InputBindingTraceDomain::Button,
        .buttonId = id,
        .buttonType = type,
        .scopeId = scope,
        .authorityScope = currentAuthority(),
        .scoped = scope != 0,
        .active = true,
        .candidateCount = selection.candidateCount,
    });
}

FLASHMEM ButtonBinding* InputBinding::findButtonBinding(
    oc::type::BindingID id
) {
    if (id == 0) return nullptr;
    for (auto& binding : button_registry_.bindings()) {
        if (binding.id == id) return &binding;
    }
    return nullptr;
}

FLASHMEM bool InputBinding::dispatchExactButtonBinding(
    oc::type::ButtonID id,
    ButtonBindingType type,
    oc::type::BindingID bindingId,
    bool recheckActive
) {
    auto* binding = findButtonBinding(bindingId);
    if (!binding || !binding->enabled || binding->type != type || !binding->action) {
        traceConsumed(id, type);
        return false;
    }

    const bool buttonMatches =
        type == ButtonBindingType::COMBO
            ? binding->buttonId == id ||
                  (binding->secondaryButton.has_value() &&
                   *binding->secondaryButton == id)
            : binding->buttonId == id;
    if (!buttonMatches) {
        traceConsumed(id, type);
        return false;
    }

    if (type == ButtonBindingType::COMBO &&
        (!binding->secondaryButton.has_value() ||
         !gesture_.isComboActive(binding->buttonId, *binding->secondaryButton))) {
        return false;
    }

    const bool active = !recheckActive || isBindingActive(*binding);
    trace({
        .stage = InputBindingTraceStage::Candidate,
        .domain = InputBindingTraceDomain::Button,
        .buttonId = id,
        .buttonType = type,
        .bindingId = binding->id,
        .scopeId = binding->scopeId,
        .authorityScope = currentAuthority(),
        .scoped = binding->scopeId != 0,
        .active = active,
        .authority = true,
    });
    if (!active) {
        traceConsumed(id, type);
        return false;
    }

    trace({
        .stage = InputBindingTraceStage::Dispatch,
        .domain = InputBindingTraceDomain::Button,
        .buttonId = id,
        .buttonType = type,
        .bindingId = binding->id,
        .scopeId = binding->scopeId,
        .authorityScope = currentAuthority(),
        .scoped = binding->scopeId != 0,
        .active = true,
        .authority = true,
        .dispatched = true,
    });
    binding->action();
    return true;
}

FLASHMEM bool InputBinding::hasActiveGlobalPassThrough(
    oc::type::ButtonID id
) {
    for (const auto& binding : button_registry_.bindings()) {
        if (!binding.enabled || binding.scopeId != 0 || !binding.globalPassThrough) continue;
        const bool buttonMatches =
            binding.type == ButtonBindingType::COMBO
                ? binding.buttonId == id ||
                      (binding.secondaryButton.has_value() &&
                       *binding.secondaryButton == id)
                : binding.buttonId == id;
        if (buttonMatches && isBindingActive(binding)) return true;
    }
    return false;
}

FLASHMEM void InputBinding::traceConsumed(
    oc::type::ButtonID id,
    ButtonBindingType type
) {
    trace({
        .stage = InputBindingTraceStage::Consumed,
        .domain = InputBindingTraceDomain::Button,
        .buttonId = id,
        .buttonType = type,
        .scopeId = gesture_routes_.route(id).ownerScope,
        .authorityScope = currentAuthority(),
        .scoped = gesture_routes_.route(id).ownerScope != 0,
    });
}

// ═══════════════════════════════════════════════════════════════════════════
// Release Handling (decomposed)
// ═══════════════════════════════════════════════════════════════════════════

void InputBinding::handleScopedRelease(oc::type::ButtonID id, oc::type::ScopeID pressOwner, uint32_t pressDuration) {
    if (shouldActivateLatch(id, pressOwner, pressDuration)) {
        latch_.activate(id, pressOwner);
    } else {
        if (!dispatchReleaseToScope(id, pressOwner, false) &&
            config_.releaseRoutingPolicy == ReleaseRoutingPolicy::OwnerThenFallback) {
            diagnostics_.legacyFallbacks++;
            trace({
                .stage = InputBindingTraceStage::Fallback,
                .domain = InputBindingTraceDomain::Button,
                .buttonId = id,
                .buttonType = ButtonBindingType::RELEASE,
                .scopeId = pressOwner,
                .authorityScope = currentAuthority(),
                .scoped = true,
            });
            dispatchButtonEvent(id, ButtonBindingType::RELEASE);
        }
    }
}

void InputBinding::handleLatchedRelease(oc::type::ButtonID id, oc::type::ScopeID latchOwner) {
    if (!dispatchReleaseToScope(id, latchOwner, false) &&
        config_.releaseRoutingPolicy == ReleaseRoutingPolicy::OwnerThenFallback) {
        diagnostics_.legacyFallbacks++;
        trace({
            .stage = InputBindingTraceStage::Fallback,
            .domain = InputBindingTraceDomain::Button,
            .buttonId = id,
            .buttonType = ButtonBindingType::RELEASE,
            .scopeId = latchOwner,
            .authorityScope = currentAuthority(),
            .scoped = true,
        });
        dispatchButtonEvent(id, ButtonBindingType::RELEASE);
    }
    latch_.release(id);
}

bool InputBinding::shouldActivateLatch(oc::type::ButtonID id, oc::type::ScopeID pressOwner, uint32_t pressDuration) const {
    if (pressDuration >= config_.latchThresholdMs) return false;

    if (config_.gestureRoutingPolicy == GestureRoutingPolicy::PressScoped) {
        const auto bindingId = gesture_routes_.route(id).pressBinding;
        if (bindingId == 0) return false;
        for (const auto& binding : button_registry_.bindings()) {
            if (binding.id == bindingId) {
                return binding.enabled && binding.latch &&
                       binding.type == ButtonBindingType::PRESS &&
                       binding.scopeId == pressOwner;
            }
        }
        return false;
    }

    for (const auto& binding : button_registry_.bindings()) {
        if (binding.buttonId == id &&
            binding.type == ButtonBindingType::PRESS &&
            binding.scopeId == pressOwner &&
            binding.latch &&
            binding.enabled) {
            return true;
        }
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// oc::type::Event Dispatch
// ═══════════════════════════════════════════════════════════════════════════

void InputBinding::dispatchButtonEvent(oc::type::ButtonID id, ButtonBindingType type) {
    if (!bindings_enabled_) return;
    bool dispatched = false;

    // Try scoped bindings first (stop after first match)
    for (auto& binding : button_registry_.bindings()) {
        if (!binding.enabled || binding.buttonId != id || binding.type != type) continue;
        if (binding.scopeId == 0) continue;
        const bool active = isBindingActive(binding);
        const bool authority = hasAuthority(binding.scopeId);
        trace({
            .stage = InputBindingTraceStage::Candidate,
            .domain = InputBindingTraceDomain::Button,
            .buttonId = id,
            .buttonType = type,
            .bindingId = binding.id,
            .scopeId = binding.scopeId,
            .authorityScope = currentAuthority(),
            .scoped = true,
            .active = active,
            .authority = authority,
        });
        if (!active || !authority) continue;

        if (binding.action) {
            trace({
                .stage = InputBindingTraceStage::Dispatch,
                .domain = InputBindingTraceDomain::Button,
                .buttonId = id,
                .buttonType = type,
                .bindingId = binding.id,
                .scopeId = binding.scopeId,
                .authorityScope = currentAuthority(),
                .scoped = true,
                .active = active,
                .authority = authority,
                .dispatched = true,
            });
            binding.action();
            dispatched = true;
            return;
        }
    }

    // Fall back to global bindings (trigger all matches)
    for (auto& binding : button_registry_.bindings()) {
        if (!binding.enabled || binding.buttonId != id || binding.type != type) continue;
        if (binding.scopeId != 0) continue;
        const bool active = isBindingActive(binding);
        trace({
            .stage = InputBindingTraceStage::Candidate,
            .domain = InputBindingTraceDomain::Button,
            .buttonId = id,
            .buttonType = type,
            .bindingId = binding.id,
            .scopeId = binding.scopeId,
            .authorityScope = currentAuthority(),
            .scoped = false,
            .active = active,
            .authority = true,
        });
        if (!active) continue;

        if (binding.action) {
            trace({
                .stage = InputBindingTraceStage::Dispatch,
                .domain = InputBindingTraceDomain::Button,
                .buttonId = id,
                .buttonType = type,
                .bindingId = binding.id,
                .scopeId = binding.scopeId,
                .authorityScope = currentAuthority(),
                .scoped = false,
                .active = active,
                .authority = true,
                .dispatched = true,
            });
            binding.action();
            dispatched = true;
        }
    }

    if (!dispatched) {
        trace({
            .stage = InputBindingTraceStage::NoDispatch,
            .domain = InputBindingTraceDomain::Button,
            .buttonId = id,
            .buttonType = type,
            .authorityScope = currentAuthority(),
        });
    }
}

void InputBinding::dispatchEncoderEvent(oc::type::EncoderID id, float value) {
    if (!bindings_enabled_) return;
    bool dispatched = false;

    // Try scoped bindings first
    for (auto& binding : encoder_registry_.bindings()) {
        if (!binding.enabled || binding.encoderId != id) continue;
        if (binding.scopeId == 0) continue;
        const bool active = isBindingActive(binding);
        const bool authority = hasAuthority(binding.scopeId);
        const bool requiredButton = checkRequiredButton(binding);
        trace({
            .stage = InputBindingTraceStage::Candidate,
            .domain = InputBindingTraceDomain::Encoder,
            .encoderId = id,
            .encoderType = binding.type,
            .bindingId = binding.id,
            .scopeId = binding.scopeId,
            .authorityScope = currentAuthority(),
            .scoped = true,
            .active = active,
            .authority = authority,
            .requiredButton = requiredButton,
            .encoderValue = value,
        });
        if (!active || !authority) continue;
        if (!requiredButton) continue;

        if (binding.action) {
            trace({
                .stage = InputBindingTraceStage::Dispatch,
                .domain = InputBindingTraceDomain::Encoder,
                .encoderId = id,
                .encoderType = binding.type,
                .bindingId = binding.id,
                .scopeId = binding.scopeId,
                .authorityScope = currentAuthority(),
                .scoped = true,
                .active = active,
                .authority = authority,
                .requiredButton = requiredButton,
                .dispatched = true,
                .encoderValue = value,
            });
            binding.action(value);
            return;
        }
    }

    // Fall back to global bindings
    for (auto& binding : encoder_registry_.bindings()) {
        if (!binding.enabled || binding.encoderId != id) continue;
        if (binding.scopeId != 0) continue;
        const bool active = isBindingActive(binding);
        const bool requiredButton = checkRequiredButton(binding);
        trace({
            .stage = InputBindingTraceStage::Candidate,
            .domain = InputBindingTraceDomain::Encoder,
            .encoderId = id,
            .encoderType = binding.type,
            .bindingId = binding.id,
            .scopeId = binding.scopeId,
            .authorityScope = currentAuthority(),
            .scoped = false,
            .active = active,
            .authority = true,
            .requiredButton = requiredButton,
            .encoderValue = value,
        });
        if (!active) continue;
        if (!requiredButton) continue;

        if (binding.action) {
            trace({
                .stage = InputBindingTraceStage::Dispatch,
                .domain = InputBindingTraceDomain::Encoder,
                .encoderId = id,
                .encoderType = binding.type,
                .bindingId = binding.id,
                .scopeId = binding.scopeId,
                .authorityScope = currentAuthority(),
                .scoped = false,
                .active = active,
                .authority = true,
                .requiredButton = requiredButton,
                .dispatched = true,
                .encoderValue = value,
            });
            binding.action(value);
            dispatched = true;
        }
    }

    if (!dispatched) {
        trace({
            .stage = InputBindingTraceStage::NoDispatch,
            .domain = InputBindingTraceDomain::Encoder,
            .encoderId = id,
            .authorityScope = currentAuthority(),
            .encoderValue = value,
        });
    }
}

oc::type::ScopeID InputBinding::dispatchPress(oc::type::ButtonID id, oc::type::ScopeID excludeScope) {
    if (!bindings_enabled_) return 0;
    bool dispatched = false;

    // Try scoped bindings first
    for (auto& binding : button_registry_.bindings()) {
        if (!binding.enabled || binding.buttonId != id) continue;
        if (binding.type != ButtonBindingType::PRESS) continue;
        if (binding.scopeId == 0) continue;
        const bool active = isBindingActive(binding);
        const bool authority = hasAuthority(binding.scopeId);
        trace({
            .stage = InputBindingTraceStage::Candidate,
            .domain = InputBindingTraceDomain::Button,
            .buttonId = id,
            .buttonType = ButtonBindingType::PRESS,
            .bindingId = binding.id,
            .scopeId = binding.scopeId,
            .authorityScope = currentAuthority(),
            .scoped = true,
            .active = active,
            .authority = authority,
        });
        if (!active || !authority) continue;
        if (excludeScope != 0 && binding.scopeId == excludeScope) continue;

        if (binding.action) {
            trace({
                .stage = InputBindingTraceStage::Dispatch,
                .domain = InputBindingTraceDomain::Button,
                .buttonId = id,
                .buttonType = ButtonBindingType::PRESS,
                .bindingId = binding.id,
                .scopeId = binding.scopeId,
                .authorityScope = currentAuthority(),
                .scoped = true,
                .active = active,
                .authority = authority,
                .dispatched = true,
            });
            binding.action();
            return binding.scopeId;
        }
    }

    // Fall back to global bindings (no ownership returned)
    for (auto& binding : button_registry_.bindings()) {
        if (!binding.enabled || binding.buttonId != id) continue;
        if (binding.type != ButtonBindingType::PRESS) continue;
        if (binding.scopeId != 0) continue;
        const bool active = isBindingActive(binding);
        trace({
            .stage = InputBindingTraceStage::Candidate,
            .domain = InputBindingTraceDomain::Button,
            .buttonId = id,
            .buttonType = ButtonBindingType::PRESS,
            .bindingId = binding.id,
            .scopeId = binding.scopeId,
            .authorityScope = currentAuthority(),
            .scoped = false,
            .active = active,
            .authority = true,
        });
        if (!active) continue;

        if (binding.action) {
            trace({
                .stage = InputBindingTraceStage::Dispatch,
                .domain = InputBindingTraceDomain::Button,
                .buttonId = id,
                .buttonType = ButtonBindingType::PRESS,
                .bindingId = binding.id,
                .scopeId = binding.scopeId,
                .authorityScope = currentAuthority(),
                .scoped = false,
                .active = active,
                .authority = true,
                .dispatched = true,
            });
            binding.action();
            dispatched = true;
        }
    }

    if (!dispatched) {
        trace({
            .stage = InputBindingTraceStage::NoDispatch,
            .domain = InputBindingTraceDomain::Button,
            .buttonId = id,
            .buttonType = ButtonBindingType::PRESS,
            .authorityScope = currentAuthority(),
        });
    }
    return 0;
}

bool InputBinding::dispatchReleaseToScope(oc::type::ButtonID id,
                                          oc::type::ScopeID scope,
                                          bool enforceAuthority) {
    for (auto& binding : button_registry_.bindings()) {
        if (!binding.enabled || binding.buttonId != id) continue;
        if (binding.type != ButtonBindingType::RELEASE) continue;
        if (binding.scopeId != scope) continue;
        const bool active = isBindingActive(binding);
        const bool authority = hasAuthority(binding.scopeId);
        trace({
            .stage = InputBindingTraceStage::Candidate,
            .domain = InputBindingTraceDomain::Button,
            .buttonId = id,
            .buttonType = ButtonBindingType::RELEASE,
            .bindingId = binding.id,
            .scopeId = binding.scopeId,
            .authorityScope = currentAuthority(),
            .scoped = binding.scopeId != 0,
            .active = active,
            .authority = authority,
        });
        if (!active) continue;
        if (enforceAuthority && !authority) continue;

        if (binding.action) {
            trace({
                .stage = InputBindingTraceStage::Dispatch,
                .domain = InputBindingTraceDomain::Button,
                .buttonId = id,
                .buttonType = ButtonBindingType::RELEASE,
                .bindingId = binding.id,
                .scopeId = binding.scopeId,
                .authorityScope = currentAuthority(),
                .scoped = binding.scopeId != 0,
                .active = active,
                .authority = authority,
                .dispatched = true,
            });
            binding.action();
            return true;
        }
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// Gesture Checks
// ═══════════════════════════════════════════════════════════════════════════

void InputBinding::checkLongPress(oc::type::ButtonID id, uint32_t now) {
    if (id >= MAX_BUTTONS) return;
    if (!gesture_.isPressed(id) || gesture_.longPressTriggered(id)) return;

    if (config_.gestureRoutingPolicy == GestureRoutingPolicy::PressScoped) {
        const auto& route = gesture_routes_.route(id);
        if (!route.active || route.consumed || route.longPressBinding == 0) return;

        auto* binding = findButtonBinding(route.longPressBinding);
        if (!binding ||
            !gesture_.checkLongPress(id, now, binding->longPressMs)) {
            return;
        }

        gesture_.markLongPressTriggered(id);
        dispatchExactButtonBinding(id,
                                   ButtonBindingType::LONG_PRESS,
                                   route.longPressBinding,
                                   true);
        return;
    }

    // Try scoped bindings first
    for (auto& binding : button_registry_.bindings()) {
        if (!binding.enabled || binding.type != ButtonBindingType::LONG_PRESS) continue;
        if (binding.buttonId != id || binding.scopeId == 0) continue;
        const bool active = isBindingActive(binding);
        const bool authority = hasAuthority(binding.scopeId);
        const bool due = gesture_.checkLongPress(id, now, binding.longPressMs);
        if (!due) continue;
        trace({
            .stage = InputBindingTraceStage::Candidate,
            .domain = InputBindingTraceDomain::Button,
            .buttonId = id,
            .buttonType = ButtonBindingType::LONG_PRESS,
            .bindingId = binding.id,
            .scopeId = binding.scopeId,
            .authorityScope = currentAuthority(),
            .scoped = true,
            .active = active,
            .authority = authority,
        });
        if (!active) continue;
        if (!authority) continue;

        gesture_.markLongPressTriggered(id);
        if (binding.action) {
            trace({
                .stage = InputBindingTraceStage::Dispatch,
                .domain = InputBindingTraceDomain::Button,
                .buttonId = id,
                .buttonType = ButtonBindingType::LONG_PRESS,
                .bindingId = binding.id,
                .scopeId = binding.scopeId,
                .authorityScope = currentAuthority(),
                .scoped = true,
                .active = active,
                .authority = authority,
                .dispatched = true,
            });
            binding.action();
            return;
        }
    }

    // Fall back to global bindings
    for (auto& binding : button_registry_.bindings()) {
        if (!binding.enabled || binding.type != ButtonBindingType::LONG_PRESS) continue;
        if (binding.buttonId != id || binding.scopeId != 0) continue;
        const bool due = gesture_.checkLongPress(id, now, binding.longPressMs);
        if (!due) continue;
        trace({
            .stage = InputBindingTraceStage::Candidate,
            .domain = InputBindingTraceDomain::Button,
            .buttonId = id,
            .buttonType = ButtonBindingType::LONG_PRESS,
            .bindingId = binding.id,
            .scopeId = binding.scopeId,
            .authorityScope = currentAuthority(),
            .scoped = false,
            .active = true,
            .authority = true,
        });

        gesture_.markLongPressTriggered(id);
        if (binding.action) {
            trace({
                .stage = InputBindingTraceStage::Dispatch,
                .domain = InputBindingTraceDomain::Button,
                .buttonId = id,
                .buttonType = ButtonBindingType::LONG_PRESS,
                .bindingId = binding.id,
                .scopeId = binding.scopeId,
                .authorityScope = currentAuthority(),
                .scoped = false,
                .active = true,
                .authority = true,
                .dispatched = true,
            });
            binding.action();
        }
    }
}

void InputBinding::checkDoubleTap(oc::type::ButtonID id, uint32_t now) {
    if (id >= MAX_BUTTONS) return;
    if (gesture_.tapCount(id) < 2) return;

    bool triggered = false;

    // Try scoped bindings first
    for (auto& binding : button_registry_.bindings()) {
        if (!binding.enabled || binding.type != ButtonBindingType::DOUBLE_TAP) continue;
        if (binding.buttonId != id || binding.scopeId == 0) continue;
        const bool active = isBindingActive(binding);
        const bool authority = hasAuthority(binding.scopeId);
        trace({
            .stage = InputBindingTraceStage::Candidate,
            .domain = InputBindingTraceDomain::Button,
            .buttonId = id,
            .buttonType = ButtonBindingType::DOUBLE_TAP,
            .bindingId = binding.id,
            .scopeId = binding.scopeId,
            .authorityScope = currentAuthority(),
            .scoped = true,
            .active = active,
            .authority = authority,
        });
        if (!active) continue;
        if (!authority) continue;

        if (gesture_.checkDoubleTap(id, now, binding.doubleTapWindowMs)) {
            if (binding.action) {
                trace({
                    .stage = InputBindingTraceStage::Dispatch,
                    .domain = InputBindingTraceDomain::Button,
                    .buttonId = id,
                    .buttonType = ButtonBindingType::DOUBLE_TAP,
                    .bindingId = binding.id,
                    .scopeId = binding.scopeId,
                    .authorityScope = currentAuthority(),
                    .scoped = true,
                    .active = active,
                    .authority = authority,
                    .dispatched = true,
                });
                binding.action();
                triggered = true;
                break;  // Stop after first scoped match
            }
        }
    }

    // Fall back to global bindings if no scoped triggered
    if (!triggered) {
        for (auto& binding : button_registry_.bindings()) {
            if (!binding.enabled || binding.type != ButtonBindingType::DOUBLE_TAP) continue;
            if (binding.buttonId != id || binding.scopeId != 0) continue;
            trace({
                .stage = InputBindingTraceStage::Candidate,
                .domain = InputBindingTraceDomain::Button,
                .buttonId = id,
                .buttonType = ButtonBindingType::DOUBLE_TAP,
                .bindingId = binding.id,
                .scopeId = binding.scopeId,
                .authorityScope = currentAuthority(),
                .scoped = false,
                .active = true,
                .authority = true,
            });

            if (gesture_.checkDoubleTap(id, now, binding.doubleTapWindowMs)) {
                if (binding.action) {
                    trace({
                        .stage = InputBindingTraceStage::Dispatch,
                        .domain = InputBindingTraceDomain::Button,
                        .buttonId = id,
                        .buttonType = ButtonBindingType::DOUBLE_TAP,
                        .bindingId = binding.id,
                        .scopeId = binding.scopeId,
                        .authorityScope = currentAuthority(),
                        .scoped = false,
                        .active = true,
                        .authority = true,
                        .dispatched = true,
                    });
                    binding.action();
                    triggered = true;
                }
            }
        }
    }

    if (triggered) {
        gesture_.resetTapCount(id);
    }
}

void InputBinding::checkCombo(oc::type::ButtonID releasedId) {
    // Try scoped bindings first
    for (auto& binding : button_registry_.bindings()) {
        if (!binding.enabled || binding.type != ButtonBindingType::COMBO) continue;
        if (binding.scopeId == 0 || !isBindingActive(binding)) continue;
        const bool authority = hasAuthority(binding.scopeId);
        if (!authority) continue;
        if (!binding.secondaryButton.has_value()) continue;

        bool isPartOfCombo = (binding.buttonId == releasedId) ||
                             (*binding.secondaryButton == releasedId);
        if (!isPartOfCombo) continue;
        trace({
            .stage = InputBindingTraceStage::Candidate,
            .domain = InputBindingTraceDomain::Button,
            .buttonId = releasedId,
            .buttonType = ButtonBindingType::COMBO,
            .bindingId = binding.id,
            .scopeId = binding.scopeId,
            .authorityScope = currentAuthority(),
            .scoped = true,
            .active = true,
            .authority = authority,
        });

        if (gesture_.isComboActive(binding.buttonId, *binding.secondaryButton)) {
            if (binding.action) {
                trace({
                    .stage = InputBindingTraceStage::Dispatch,
                    .domain = InputBindingTraceDomain::Button,
                    .buttonId = releasedId,
                    .buttonType = ButtonBindingType::COMBO,
                    .bindingId = binding.id,
                    .scopeId = binding.scopeId,
                    .authorityScope = currentAuthority(),
                    .scoped = true,
                    .active = true,
                    .authority = authority,
                    .dispatched = true,
                });
                binding.action();
                return;
            }
        }
    }

    // Fall back to global bindings
    for (auto& binding : button_registry_.bindings()) {
        if (!binding.enabled || binding.type != ButtonBindingType::COMBO) continue;
        if (binding.scopeId != 0) continue;
        if (!binding.secondaryButton.has_value()) continue;

        bool isPartOfCombo = (binding.buttonId == releasedId) ||
                             (*binding.secondaryButton == releasedId);
        if (!isPartOfCombo) continue;
        trace({
            .stage = InputBindingTraceStage::Candidate,
            .domain = InputBindingTraceDomain::Button,
            .buttonId = releasedId,
            .buttonType = ButtonBindingType::COMBO,
            .bindingId = binding.id,
            .scopeId = binding.scopeId,
            .authorityScope = currentAuthority(),
            .scoped = false,
            .active = true,
            .authority = true,
        });

        if (gesture_.isComboActive(binding.buttonId, *binding.secondaryButton)) {
            if (binding.action) {
                trace({
                    .stage = InputBindingTraceStage::Dispatch,
                    .domain = InputBindingTraceDomain::Button,
                    .buttonId = releasedId,
                    .buttonType = ButtonBindingType::COMBO,
                    .bindingId = binding.id,
                    .scopeId = binding.scopeId,
                    .authorityScope = currentAuthority(),
                    .scoped = false,
                    .active = true,
                    .authority = true,
                    .dispatched = true,
                });
                binding.action();
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Binding Helpers
// ═══════════════════════════════════════════════════════════════════════════

template <typename BindingType>
bool InputBinding::isBindingActive(const BindingType& binding) const {
    if (!binding.isActive) return true;
    return binding.isActive();
}

template bool InputBinding::isBindingActive(const ButtonBinding&) const;
template bool InputBinding::isBindingActive(const EncoderBinding&) const;

bool InputBinding::hasAuthority(oc::type::ScopeID scope) const {
    if (!authority_resolver_) return true;

    oc::type::ScopeID authority = authority_resolver_->getAuthority();
    if (authority == 0) return true;

    return scope == authority;
}

bool InputBinding::checkRequiredButton(const EncoderBinding& binding) const {
    if (binding.type != EncoderBindingType::TURN_WHILE_PRESSED) return true;
    if (!binding.requiredButton.has_value()) return true;

    oc::type::ButtonID btn = *binding.requiredButton;
    return gesture_.isPressed(btn) || latch_.isLatched(btn);
}

void InputBinding::trace(const InputBindingTraceEvent& event) const {
    if (trace_callback_) {
        trace_callback_(event);
    }
}

oc::type::ScopeID InputBinding::currentAuthority() const {
    return authority_resolver_ ? authority_resolver_->getAuthority() : 0;
}

}  // namespace oc::core::input
