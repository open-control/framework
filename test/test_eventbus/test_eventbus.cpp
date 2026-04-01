#include <unity.h>
#include <oc/core/event/EventBus.hpp>
#include <oc/core/event/Events.hpp>

using namespace oc::core::event;

static int callCount = 0;
static oc::type::ButtonID lastButtonId = 0;
static int secondaryCallCount = 0;

namespace {

class TestEvent : public oc::type::Event {
public:
    TestEvent(oc::type::EventCategoryType category, oc::type::EventType type)
        : oc::type::Event(category, type) {}
};

}  // namespace

void setUp() {
    callCount = 0;
    lastButtonId = 0;
    secondaryCallCount = 0;
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

void test_max_subscribers_per_topic_is_bounded() {
    EventBus bus;

    for (size_t i = 0; i < EventBus::maxSubscribersPerEvent(); ++i) {
        auto id = bus.on(EventCategory::USER_INPUT, InputEvent::BUTTON_PRESS,
            [](const oc::type::Event&) {});
        TEST_ASSERT_GREATER_THAN(0, id);
    }

    auto overflowId = bus.on(EventCategory::USER_INPUT, InputEvent::BUTTON_PRESS,
        [](const oc::type::Event&) {});

    TEST_ASSERT_EQUAL(0, overflowId);
    TEST_ASSERT_EQUAL(EventBus::maxSubscribersPerEvent(), bus.getSubscriberCount());
}

void test_dead_slots_are_reused_before_compaction() {
    EventBus bus;
    auto firstId = bus.on(EventCategory::USER_INPUT, InputEvent::BUTTON_PRESS,
        [](const oc::type::Event&) {});
    auto secondId = bus.on(EventCategory::USER_INPUT, InputEvent::BUTTON_PRESS,
        [](const oc::type::Event&) {});

    TEST_ASSERT_GREATER_THAN(0, firstId);
    TEST_ASSERT_GREATER_THAN(0, secondId);

    bus.off(firstId);
    TEST_ASSERT_EQUAL(1, bus.getSubscriberCount());

    auto reusedId = bus.on(EventCategory::USER_INPUT, InputEvent::BUTTON_PRESS,
        [](const oc::type::Event&) {});

    TEST_ASSERT_GREATER_THAN(0, reusedId);
    TEST_ASSERT_EQUAL(2, bus.getSubscriberCount());
}

void test_max_topics_is_bounded() {
    EventBus bus;

    for (size_t i = 0; i < EventBus::maxTopics(); ++i) {
        auto id = bus.on(EventCategory::UI,
                         static_cast<oc::type::EventType>(1000 + i),
                         [](const oc::type::Event&) {});
        TEST_ASSERT_GREATER_THAN(0, id);
    }

    auto overflowId = bus.on(EventCategory::UI,
                             static_cast<oc::type::EventType>(5000),
                             [](const oc::type::Event&) {});

    TEST_ASSERT_EQUAL(0, overflowId);
    TEST_ASSERT_EQUAL(EventBus::maxTopics(), bus.getSubscriberCount());
}

void test_off_during_emit_is_safe_and_skips_later_callback() {
    EventBus bus;
    oc::interface::SubscriptionID secondId = 0;

    bus.on(EventCategory::USER_INPUT, InputEvent::BUTTON_PRESS,
        [&](const oc::type::Event&) {
            callCount++;
            bus.off(secondId);
        });

    secondId = bus.on(EventCategory::USER_INPUT, InputEvent::BUTTON_PRESS,
        [](const oc::type::Event&) { secondaryCallCount++; });

    bus.emit(ButtonPressEvent{7, true});

    TEST_ASSERT_EQUAL(1, callCount);
    TEST_ASSERT_EQUAL(0, secondaryCallCount);

    bus.emit(ButtonPressEvent{8, true});

    TEST_ASSERT_EQUAL(2, callCount);
    TEST_ASSERT_EQUAL(0, secondaryCallCount);
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
    RUN_TEST(test_max_subscribers_per_topic_is_bounded);
    RUN_TEST(test_dead_slots_are_reused_before_compaction);
    RUN_TEST(test_max_topics_is_bounded);
    RUN_TEST(test_off_during_emit_is_safe_and_skips_later_callback);
    UNITY_END();
    return 0;
}
