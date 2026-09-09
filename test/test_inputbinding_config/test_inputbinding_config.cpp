#include <unity.h>

#include <oc/core/event/Events.hpp>
#include <oc/core/input/InputBinding.hpp>
#include "../mocks/MockEventBus.hpp"
#include "../mocks/FakeTime.hpp"

using namespace oc::core::input;
using namespace oc::core::event;
using namespace oc::test;

void setUp() {}
void tearDown() {}

void test_long_press_uses_owned_configuration() {
    for (auto policy : {GestureRoutingPolicy::Legacy, GestureRoutingPolicy::PressScoped}) {
        MockEventBus bus;
        FakeTime clock;
        InputConfig source;
        source.gestureRoutingPolicy = policy;
        source.longPressMs = 120;
        InputBinding input(bus, clock.provider(), source);
        int calls = 0;
        ButtonBinding button{};
        button.buttonId = 1;
        button.type = ButtonBindingType::LONG_PRESS;
        button.action = [&] { ++calls; };
        input.registerButtonBinding(button);

        // Constructor configuration is a snapshot, not a live external reference.
        source.longPressMs = 10000;
        TEST_ASSERT_EQUAL_UINT32(120, input.config().longPressMs);
        bus.emit(ButtonPressEvent{1, true});
        clock.advance(119);
        input.processTick();
        TEST_ASSERT_EQUAL_INT(0, calls);
        clock.advance(1);
        input.processTick();
        TEST_ASSERT_EQUAL_INT(1, calls);
        clock.advance(1000);
        input.processTick();
        TEST_ASSERT_EQUAL_INT(1, calls);
    }
}

void test_double_tap_uses_owned_configuration() {
    for (auto policy : {GestureRoutingPolicy::Legacy, GestureRoutingPolicy::PressScoped}) {
        MockEventBus bus;
        FakeTime clock;
        InputConfig source;
        source.gestureRoutingPolicy = policy;
        source.doubleTapWindowMs = 300;
        InputBinding input(bus, clock.provider(), source);
        int calls = 0;
        ButtonBinding button{};
        button.buttonId = 1;
        button.type = ButtonBindingType::DOUBLE_TAP;
        button.action = [&] { ++calls; };
        input.registerButtonBinding(button);

        source.doubleTapWindowMs = 1;
        clock.advance(1000);
        input.processTick();
        bus.emit(ButtonPressEvent{1, true});
        clock.advance(10);
        input.processTick();
        bus.emit(ButtonReleaseEvent{1});
        clock.advance(100);
        input.processTick();
        bus.emit(ButtonPressEvent{1, true});
        clock.advance(10);
        input.processTick();
        bus.emit(ButtonReleaseEvent{1});
        TEST_ASSERT_EQUAL_INT(1, calls);
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_long_press_uses_owned_configuration);
    RUN_TEST(test_double_tap_uses_owned_configuration);
    return UNITY_END();
}
