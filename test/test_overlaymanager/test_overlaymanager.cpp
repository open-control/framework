#include <unity.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <oc/context/OverlayManager.hpp>
#include <oc/core/event/Events.hpp>
#include <oc/core/input/InputBinding.hpp>
#include <oc/interface/IButton.hpp>
#include <oc/state/ExclusiveVisibilityStack.hpp>
#include <oc/state/Signal.hpp>

#include "../mocks/FakeTime.hpp"
#include "../mocks/MockEventBus.hpp"

namespace {

enum class Overlay : uint8_t {
    NONE = 0,
    A,
    B,
    C,
    COUNT,
};

struct PresentationEvent {
    Overlay type = Overlay::NONE;
    bool presented = false;
};

struct PresentationTrace {
    std::array<PresentationEvent, 8> events{};
    size_t count = 0;
};

void recordPresentation(void* context, Overlay type, bool presented) {
    auto* trace = static_cast<PresentationTrace*>(context);
    if (!trace || trace->count >= trace->events.size()) return;
    trace->events[trace->count++] = {.type = type, .presented = presented};
}

class DummyButton final : public oc::interface::IButton {
public:
    oc::type::Result<void> init() override { return oc::type::Result<void>::ok(); }
    void update(uint32_t) override {}
    bool isPressed(oc::type::ButtonID) const override { return false; }
    void setCallback(oc::type::ButtonCallback) override {}
};

}  // namespace

static oc::test::MockEventBus eventBus;
static oc::test::FakeTime fakeTime;

void setUp() {
    eventBus.reset();
    fakeTime.reset();
}
void tearDown() {}

void test_presentation_follows_replacement_and_stacked_hide() {
    oc::state::ExclusiveVisibilityStack<Overlay> stack;
    oc::state::Signal<bool> a{false};
    oc::state::Signal<bool> b{false};
    oc::state::Signal<bool> c{false};
    stack.registerItem(Overlay::A, a);
    stack.registerItem(Overlay::B, b);
    stack.registerItem(Overlay::C, c);

    oc::context::OverlayManager<Overlay> manager(stack);
    PresentationTrace trace{};
    manager.setPresentationCallback(&trace, recordPresentation);

    manager.show(Overlay::A);
    manager.show(Overlay::B);
    manager.show(Overlay::C, true);
    manager.hide();
    manager.hide();

    TEST_ASSERT_EQUAL_UINT32(6, trace.count);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Overlay::A), static_cast<int>(trace.events[0].type));
    TEST_ASSERT_TRUE(trace.events[0].presented);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Overlay::A), static_cast<int>(trace.events[1].type));
    TEST_ASSERT_FALSE(trace.events[1].presented);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Overlay::B), static_cast<int>(trace.events[2].type));
    TEST_ASSERT_TRUE(trace.events[2].presented);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Overlay::C), static_cast<int>(trace.events[3].type));
    TEST_ASSERT_TRUE(trace.events[3].presented);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Overlay::C), static_cast<int>(trace.events[4].type));
    TEST_ASSERT_FALSE(trace.events[4].presented);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Overlay::B), static_cast<int>(trace.events[5].type));
    TEST_ASSERT_FALSE(trace.events[5].presented);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Overlay::NONE), static_cast<int>(manager.current()));
}

void test_authority_transition_quarantines_opening_button_release() {
    oc::core::input::InputConfig config;
    config.gestureRoutingPolicy = oc::core::input::GestureRoutingPolicy::PressScoped;
    config.releaseRoutingPolicy = oc::core::input::ReleaseRoutingPolicy::OwnerOnly;
    config.ambiguityPolicy = oc::core::input::BindingAmbiguityPolicy::FailClosed;
    config.globalRoutingPolicy =
        oc::core::input::GlobalRoutingPolicy::ExplicitPassThroughOnly;

    oc::core::input::InputBinding input(eventBus, fakeTime.provider(), config);
    DummyButton hardware;
    oc::api::ButtonAPI buttons(input, hardware);
    oc::state::ExclusiveVisibilityStack<Overlay> stack;
    oc::state::Signal<bool> a{false};
    stack.registerItem(Overlay::A, a);
    oc::context::OverlayManager<Overlay> manager(stack, buttons);
    manager.registerCleanup(Overlay::A, 200);
    manager.setActiveViewProvider([]() { return oc::type::ScopeID(100); });

    int releaseCount = 0;
    oc::core::input::ButtonBinding press{};
    press.type = oc::core::input::ButtonBindingType::PRESS;
    press.buttonId = 1;
    press.scopeId = 100;
    press.action = [&]() { manager.show(Overlay::A); };
    input.registerButtonBinding(press);

    oc::core::input::ButtonBinding release{};
    release.type = oc::core::input::ButtonBindingType::RELEASE;
    release.buttonId = 1;
    release.scopeId = 100;
    release.action = [&]() { releaseCount++; };
    input.registerButtonBinding(release);

    eventBus.emit(oc::core::event::ButtonPressEvent{1, true});
    eventBus.emit(oc::core::event::ButtonReleaseEvent{1});

    TEST_ASSERT_EQUAL(0, releaseCount);
    TEST_ASSERT_EQUAL_UINT32(1, input.diagnostics().quarantinedGestures);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Overlay::A),
                          static_cast<int>(manager.current()));
}

void test_authority_transition_quarantines_closing_button_release() {
    oc::core::input::InputConfig config;
    config.gestureRoutingPolicy = oc::core::input::GestureRoutingPolicy::PressScoped;
    config.releaseRoutingPolicy = oc::core::input::ReleaseRoutingPolicy::OwnerOnly;
    config.ambiguityPolicy = oc::core::input::BindingAmbiguityPolicy::FailClosed;
    config.globalRoutingPolicy =
        oc::core::input::GlobalRoutingPolicy::ExplicitPassThroughOnly;

    oc::core::input::InputBinding input(eventBus, fakeTime.provider(), config);
    DummyButton hardware;
    oc::api::ButtonAPI buttons(input, hardware);
    oc::state::ExclusiveVisibilityStack<Overlay> stack;
    oc::state::Signal<bool> a{false};
    stack.registerItem(Overlay::A, a);
    oc::context::OverlayManager<Overlay> manager(stack, buttons);
    manager.registerCleanup(Overlay::A, 200);
    manager.setActiveViewProvider([]() { return oc::type::ScopeID(100); });
    manager.show(Overlay::A);

    int releaseCount = 0;
    oc::core::input::ButtonBinding press{};
    press.type = oc::core::input::ButtonBindingType::PRESS;
    press.buttonId = 1;
    press.scopeId = 200;
    press.action = [&]() { manager.hide(); };
    input.registerButtonBinding(press);

    oc::core::input::ButtonBinding release{};
    release.type = oc::core::input::ButtonBindingType::RELEASE;
    release.buttonId = 1;
    release.scopeId = 200;
    release.action = [&]() { releaseCount++; };
    input.registerButtonBinding(release);

    eventBus.emit(oc::core::event::ButtonPressEvent{1, true});
    eventBus.emit(oc::core::event::ButtonReleaseEvent{1});

    TEST_ASSERT_EQUAL(0, releaseCount);
    TEST_ASSERT_EQUAL_UINT32(1, input.diagnostics().quarantinedGestures);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Overlay::NONE),
                          static_cast<int>(manager.current()));
}

void test_direct_stack_transition_is_also_quarantined() {
    oc::core::input::InputConfig config;
    config.gestureRoutingPolicy = oc::core::input::GestureRoutingPolicy::PressScoped;
    config.releaseRoutingPolicy = oc::core::input::ReleaseRoutingPolicy::OwnerOnly;
    config.ambiguityPolicy = oc::core::input::BindingAmbiguityPolicy::FailClosed;
    config.globalRoutingPolicy =
        oc::core::input::GlobalRoutingPolicy::ExplicitPassThroughOnly;

    oc::core::input::InputBinding input(eventBus, fakeTime.provider(), config);
    DummyButton hardware;
    oc::api::ButtonAPI buttons(input, hardware);
    oc::state::ExclusiveVisibilityStack<Overlay> stack;
    oc::state::Signal<bool> a{false};
    stack.registerItem(Overlay::A, a);
    oc::context::OverlayManager<Overlay> manager(stack, buttons);
    manager.registerCleanup(Overlay::A, 200);
    manager.setActiveViewProvider([]() { return oc::type::ScopeID(100); });

    int releaseCount = 0;
    oc::core::input::ButtonBinding press{};
    press.type = oc::core::input::ButtonBindingType::PRESS;
    press.buttonId = 1;
    press.scopeId = 100;
    press.action = [&]() { stack.show(Overlay::A); };
    input.registerButtonBinding(press);

    oc::core::input::ButtonBinding release{};
    release.type = oc::core::input::ButtonBindingType::RELEASE;
    release.buttonId = 1;
    release.scopeId = 100;
    release.action = [&]() { releaseCount++; };
    input.registerButtonBinding(release);

    eventBus.emit(oc::core::event::ButtonPressEvent{1, true});
    eventBus.emit(oc::core::event::ButtonReleaseEvent{1});

    TEST_ASSERT_EQUAL(0, releaseCount);
    TEST_ASSERT_EQUAL_UINT32(1, input.diagnostics().quarantinedGestures);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Overlay::A),
                          static_cast<int>(manager.current()));
}

void test_idempotent_overlay_show_keeps_current_gesture_route() {
    oc::core::input::InputConfig config;
    config.gestureRoutingPolicy = oc::core::input::GestureRoutingPolicy::PressScoped;
    config.releaseRoutingPolicy = oc::core::input::ReleaseRoutingPolicy::OwnerOnly;
    config.ambiguityPolicy = oc::core::input::BindingAmbiguityPolicy::FailClosed;
    config.globalRoutingPolicy =
        oc::core::input::GlobalRoutingPolicy::ExplicitPassThroughOnly;

    oc::core::input::InputBinding input(eventBus, fakeTime.provider(), config);
    DummyButton hardware;
    oc::api::ButtonAPI buttons(input, hardware);
    oc::state::ExclusiveVisibilityStack<Overlay> stack;
    oc::state::Signal<bool> a{false};
    stack.registerItem(Overlay::A, a);
    oc::context::OverlayManager<Overlay> manager(stack, buttons);
    manager.registerCleanup(Overlay::A, 200);
    manager.setActiveViewProvider([]() { return oc::type::ScopeID(100); });
    manager.show(Overlay::A);

    int releaseCount = 0;
    oc::core::input::ButtonBinding press{};
    press.type = oc::core::input::ButtonBindingType::PRESS;
    press.buttonId = 1;
    press.scopeId = 200;
    press.action = [&]() { manager.show(Overlay::A); };
    input.registerButtonBinding(press);

    oc::core::input::ButtonBinding release{};
    release.type = oc::core::input::ButtonBindingType::RELEASE;
    release.buttonId = 1;
    release.scopeId = 200;
    release.action = [&]() { releaseCount++; };
    input.registerButtonBinding(release);

    eventBus.emit(oc::core::event::ButtonPressEvent{1, true});
    eventBus.emit(oc::core::event::ButtonReleaseEvent{1});

    TEST_ASSERT_EQUAL(1, releaseCount);
    TEST_ASSERT_EQUAL_UINT32(0, input.diagnostics().quarantinedGestures);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Overlay::A),
                          static_cast<int>(manager.current()));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_presentation_follows_replacement_and_stacked_hide);
    RUN_TEST(test_authority_transition_quarantines_opening_button_release);
    RUN_TEST(test_authority_transition_quarantines_closing_button_release);
    RUN_TEST(test_direct_stack_transition_is_also_quarantined);
    RUN_TEST(test_idempotent_overlay_show_keeps_current_gesture_route);
    return UNITY_END();
}
