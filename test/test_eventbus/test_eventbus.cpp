#include <unity.h>
#include <oc/core/event/EventBus.hpp>
#include <oc/core/event/Events.hpp>

using namespace oc::core::event;

static int callCount = 0;
static oc::type::ButtonID lastButtonId = 0;

void setUp() {
    callCount = 0;
    lastButtonId = 0;
}

void tearDown() {}

void test_on_and_emit() {
    EventBus bus;

    auto id = bus.on(EventCategory::USER_INPUT, InputEvent::BUTTON_PRESS,
        [](const oc::type::Event& e) {
            callCount++;
            auto& btn = static_cast<const ButtonPressEvent&>(e);
            lastButtonId = btn.buttonId;
        });

    bus.emit(ButtonPressEvent{42, true});

    TEST_ASSERT_EQUAL(1, callCount);
    TEST_ASSERT_EQUAL(42, lastButtonId);
    TEST_ASSERT_GREATER_THAN(0, id);
}

void test_off_unsubscribes() {
    EventBus bus;

    auto id = bus.on(EventCategory::USER_INPUT, InputEvent::BUTTON_PRESS,
        [](const oc::type::Event&) { callCount++; });

    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(1, callCount);

    bus.off(id);

    bus.emit(ButtonPressEvent{2, true});
    TEST_ASSERT_EQUAL(1, callCount); // should not increment
}

void test_multiple_subscribers() {
    EventBus bus;

    bus.on(EventCategory::USER_INPUT, InputEvent::BUTTON_PRESS,
        [](const oc::type::Event&) { callCount++; });
    bus.on(EventCategory::USER_INPUT, InputEvent::BUTTON_PRESS,
        [](const oc::type::Event&) { callCount++; });

    bus.emit(ButtonPressEvent{1, true});

    TEST_ASSERT_EQUAL(2, callCount);
}

void test_category_filtering() {
    EventBus bus;

    // Subscribe to MIDI events
    bus.on(EventCategory::MIDI, MidiEvent::CC,
        [](const oc::type::Event&) { callCount++; });

    // Emit USER_INPUT event - should NOT trigger
    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(0, callCount);

    // Emit MIDI event - should trigger
    bus.emit(MidiCCEvent{1, 10, 127});
    TEST_ASSERT_EQUAL(1, callCount);
}

void test_type_filtering() {
    EventBus bus;

    // Subscribe to BUTTON_PRESS only
    bus.on(EventCategory::USER_INPUT, InputEvent::BUTTON_PRESS,
        [](const oc::type::Event&) { callCount++; });

    // Emit BUTTON_RELEASE - should NOT trigger
    bus.emit(ButtonReleaseEvent{1});
    TEST_ASSERT_EQUAL(0, callCount);

    // Emit BUTTON_PRESS - should trigger
    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(1, callCount);
}

void test_clear_removes_all() {
    EventBus bus;

    bus.on(EventCategory::USER_INPUT, InputEvent::BUTTON_PRESS,
        [](const oc::type::Event&) { callCount++; });
    bus.on(EventCategory::MIDI, MidiEvent::CC,
        [](const oc::type::Event&) { callCount++; });

    TEST_ASSERT_EQUAL(2, bus.getSubscriberCount());

    bus.clear();

    TEST_ASSERT_EQUAL(0, bus.getSubscriberCount());

    bus.emit(ButtonPressEvent{1, true});
    bus.emit(MidiCCEvent{1, 10, 127});
    TEST_ASSERT_EQUAL(0, callCount);
}

void test_subscriber_count() {
    EventBus bus;

    TEST_ASSERT_EQUAL(0, bus.getSubscriberCount());

    auto id1 = bus.on(EventCategory::USER_INPUT, InputEvent::BUTTON_PRESS,
        [](const oc::type::Event&) {});
    TEST_ASSERT_EQUAL(1, bus.getSubscriberCount());

    auto id2 = bus.on(EventCategory::MIDI, MidiEvent::CC,
        [](const oc::type::Event&) {});
    TEST_ASSERT_EQUAL(2, bus.getSubscriberCount());

    bus.off(id1);
    TEST_ASSERT_EQUAL(1, bus.getSubscriberCount());

    bus.off(id2);
    TEST_ASSERT_EQUAL(0, bus.getSubscriberCount());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_on_and_emit);
    RUN_TEST(test_off_unsubscribes);
    RUN_TEST(test_multiple_subscribers);
    RUN_TEST(test_category_filtering);
    RUN_TEST(test_type_filtering);
    RUN_TEST(test_clear_removes_all);
    RUN_TEST(test_subscriber_count);
    UNITY_END();
    return 0;
}
