#include <unity.h>
#include <oc/core/input/InputBinding.hpp>
#include <oc/core/event/Events.hpp>
#include "../mocks/MockEventBus.hpp"
#include "../mocks/FakeTime.hpp"

using namespace oc::core::input;
using namespace oc::core::event;
using namespace oc::core;
using namespace oc::test;

static MockEventBus bus;
static FakeTime fakeTime;
static int actionCount = 0;
static float lastEncoderValue = 0.0f;

void setUp() {
    bus.reset();
    fakeTime.reset();
    actionCount = 0;
    lastEncoderValue = 0.0f;
}

void tearDown() {}

// ═══════════════════════════════════════════════════════════════════
// Basic Registration
// ═══════════════════════════════════════════════════════════════════

void test_register_button_binding_returns_id() {
    InputBinding binding(bus, fakeTime.provider());

    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.action = []() { actionCount++; };

    BindingID id = binding.registerButtonBinding(btn);
    TEST_ASSERT_GREATER_THAN(0, id);
}

void test_register_encoder_binding_returns_id() {
    InputBinding binding(bus, fakeTime.provider());

    EncoderBinding enc{};
    enc.type = EncoderBindingType::TURN;
    enc.encoderId = 0;
    enc.action = [](float v) { lastEncoderValue = v; };

    BindingID id = binding.registerEncoderBinding(enc);
    TEST_ASSERT_GREATER_THAN(0, id);
}

void test_sequential_ids() {
    InputBinding binding(bus, fakeTime.provider());

    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.action = []() {};

    BindingID id1 = binding.registerButtonBinding(btn);
    BindingID id2 = binding.registerButtonBinding(btn);
    BindingID id3 = binding.registerButtonBinding(btn);

    TEST_ASSERT_EQUAL(id1 + 1, id2);
    TEST_ASSERT_EQUAL(id2 + 1, id3);
}

// ═══════════════════════════════════════════════════════════════════
// Button Press Events
// ═══════════════════════════════════════════════════════════════════

void test_button_press_triggers_action() {
    InputBinding binding(bus, fakeTime.provider());

    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    bus.emit(ButtonPressEvent{1, true});

    TEST_ASSERT_EQUAL(1, actionCount);
}

void test_button_press_wrong_id_no_trigger() {
    InputBinding binding(bus, fakeTime.provider());

    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    bus.emit(ButtonPressEvent{2, true});  // Different button

    TEST_ASSERT_EQUAL(0, actionCount);
}

void test_button_release_triggers_action() {
    InputBinding binding(bus, fakeTime.provider());

    ButtonBinding btn{};
    btn.type = ButtonBindingType::RELEASE;
    btn.buttonId = 1;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    // Press then release
    bus.emit(ButtonPressEvent{1, true});
    bus.emit(ButtonReleaseEvent{1});

    TEST_ASSERT_EQUAL(1, actionCount);
}

// ═══════════════════════════════════════════════════════════════════
// Long Press Detection
// ═══════════════════════════════════════════════════════════════════

void test_long_press_triggers_after_duration() {
    InputConfig config;
    config.longPressMs = 500;
    InputBinding binding(bus, fakeTime.provider(), config);

    ButtonBinding btn{};
    btn.type = ButtonBindingType::LONG_PRESS;
    btn.buttonId = 1;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    // Press button
    fakeTime.set(0);
    binding.processTick();
    bus.emit(ButtonPressEvent{1, true});

    // Not enough time
    fakeTime.set(400);
    binding.processTick();
    TEST_ASSERT_EQUAL(0, actionCount);

    // Enough time
    fakeTime.set(600);
    binding.processTick();
    TEST_ASSERT_EQUAL(1, actionCount);
}

void test_long_press_only_triggers_once() {
    InputConfig config;
    config.longPressMs = 500;
    InputBinding binding(bus, fakeTime.provider(), config);

    ButtonBinding btn{};
    btn.type = ButtonBindingType::LONG_PRESS;
    btn.buttonId = 1;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    fakeTime.set(0);
    binding.processTick();
    bus.emit(ButtonPressEvent{1, true});

    fakeTime.set(600);
    binding.processTick();
    binding.processTick();
    binding.processTick();

    TEST_ASSERT_EQUAL(1, actionCount);  // Should only trigger once
}

void test_long_press_custom_duration() {
    InputConfig config;
    config.longPressMs = 500;  // Default
    InputBinding binding(bus, fakeTime.provider(), config);

    ButtonBinding btn{};
    btn.type = ButtonBindingType::LONG_PRESS;
    btn.buttonId = 1;
    btn.longPressMs = 1000;  // Custom: 1 second
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    fakeTime.set(0);
    binding.processTick();
    bus.emit(ButtonPressEvent{1, true});

    fakeTime.set(600);
    binding.processTick();
    TEST_ASSERT_EQUAL(0, actionCount);  // Not yet

    fakeTime.set(1100);
    binding.processTick();
    TEST_ASSERT_EQUAL(1, actionCount);  // Now triggers
}

// ═══════════════════════════════════════════════════════════════════
// Double Tap Detection
// ═══════════════════════════════════════════════════════════════════

void test_double_tap_triggers() {
    InputConfig config;
    config.doubleTapWindowMs = 300;
    InputBinding binding(bus, fakeTime.provider(), config);

    ButtonBinding btn{};
    btn.type = ButtonBindingType::DOUBLE_TAP;
    btn.buttonId = 1;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    // First tap
    fakeTime.set(0);
    binding.processTick();
    bus.emit(ButtonPressEvent{1, true});
    fakeTime.set(50);
    binding.processTick();
    bus.emit(ButtonReleaseEvent{1});

    // Second tap within window
    fakeTime.set(150);
    binding.processTick();
    bus.emit(ButtonPressEvent{1, true});
    fakeTime.set(200);
    binding.processTick();
    bus.emit(ButtonReleaseEvent{1});

    TEST_ASSERT_EQUAL(1, actionCount);
}

void test_double_tap_outside_window_no_trigger() {
    InputConfig config;
    config.doubleTapWindowMs = 300;
    InputBinding binding(bus, fakeTime.provider(), config);

    ButtonBinding btn{};
    btn.type = ButtonBindingType::DOUBLE_TAP;
    btn.buttonId = 1;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    // First tap
    fakeTime.set(0);
    binding.processTick();
    bus.emit(ButtonPressEvent{1, true});
    fakeTime.set(50);
    binding.processTick();
    bus.emit(ButtonReleaseEvent{1});

    // Second tap OUTSIDE window
    fakeTime.set(500);
    binding.processTick();
    bus.emit(ButtonPressEvent{1, true});
    fakeTime.set(550);
    binding.processTick();
    bus.emit(ButtonReleaseEvent{1});

    TEST_ASSERT_EQUAL(0, actionCount);
}

// ═══════════════════════════════════════════════════════════════════
// Encoder Events
// ═══════════════════════════════════════════════════════════════════

void test_encoder_turn_triggers_action() {
    InputBinding binding(bus, fakeTime.provider());

    EncoderBinding enc{};
    enc.type = EncoderBindingType::TURN;
    enc.encoderId = 0;
    enc.action = [](float v) {
        actionCount++;
        lastEncoderValue = v;
    };
    binding.registerEncoderBinding(enc);

    bus.emit(EncoderChangedEvent{0, 0.75f});

    TEST_ASSERT_EQUAL(1, actionCount);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.75f, lastEncoderValue);
}

void test_encoder_turn_while_pressed() {
    InputBinding binding(bus, fakeTime.provider());

    EncoderBinding enc{};
    enc.type = EncoderBindingType::TURN_WHILE_PRESSED;
    enc.encoderId = 0;
    enc.requiredButton = 1;
    enc.action = [](float v) { actionCount++; };
    binding.registerEncoderBinding(enc);

    // Turn without button pressed - no trigger
    bus.emit(EncoderChangedEvent{0, 0.5f});
    TEST_ASSERT_EQUAL(0, actionCount);

    // Press button, then turn
    bus.emit(ButtonPressEvent{1, true});
    bus.emit(EncoderChangedEvent{0, 0.6f});
    TEST_ASSERT_EQUAL(1, actionCount);
}

// ═══════════════════════════════════════════════════════════════════
// Remove Binding
// ═══════════════════════════════════════════════════════════════════

void test_remove_by_id() {
    InputBinding binding(bus, fakeTime.provider());

    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.action = []() { actionCount++; };
    BindingID id = binding.registerButtonBinding(btn);

    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(1, actionCount);

    bool removed = binding.removeById(id);
    TEST_ASSERT_TRUE(removed);

    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(1, actionCount);  // No longer triggers
}

void test_remove_invalid_id() {
    InputBinding binding(bus, fakeTime.provider());

    bool removed = binding.removeById(9999);
    TEST_ASSERT_FALSE(removed);
}

// ═══════════════════════════════════════════════════════════════════
// Clear Bindings
// ═══════════════════════════════════════════════════════════════════

void test_clear_all_bindings() {
    InputBinding binding(bus, fakeTime.provider());

    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);
    binding.registerButtonBinding(btn);

    binding.clearBindings();

    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(0, actionCount);
}

void test_clear_button_bindings_only() {
    InputBinding binding(bus, fakeTime.provider());

    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    EncoderBinding enc{};
    enc.type = EncoderBindingType::TURN;
    enc.encoderId = 0;
    enc.action = [](float) { actionCount += 10; };
    binding.registerEncoderBinding(enc);

    binding.clearButtonBindings();

    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(0, actionCount);

    bus.emit(EncoderChangedEvent{0, 0.5f});
    TEST_ASSERT_EQUAL(10, actionCount);  // Encoder still works
}

// ═══════════════════════════════════════════════════════════════════
// Scoped Bindings
// ═══════════════════════════════════════════════════════════════════

void test_scoped_binding_priority() {
    InputBinding binding(bus, fakeTime.provider());
    int globalCount = 0;
    int scopedCount = 0;

    // Global binding
    ButtonBinding global{};
    global.type = ButtonBindingType::PRESS;
    global.buttonId = 1;
    global.scopeId = 0;  // Global
    global.action = [&]() { globalCount++; };
    binding.registerButtonBinding(global);

    // Scoped binding (higher priority)
    ButtonBinding scoped{};
    scoped.type = ButtonBindingType::PRESS;
    scoped.buttonId = 1;
    scoped.scopeId = 42;  // Scoped
    scoped.action = [&]() { scopedCount++; };
    binding.registerButtonBinding(scoped);

    bus.emit(ButtonPressEvent{1, true});

    TEST_ASSERT_EQUAL(1, scopedCount);
    TEST_ASSERT_EQUAL(0, globalCount);  // Blocked by scoped
}

void test_clear_scope() {
    InputBinding binding(bus, fakeTime.provider());

    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.scopeId = 42;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);
    binding.registerButtonBinding(btn);

    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(2, actionCount);

    binding.clearScope(42);

    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(2, actionCount);  // No more triggers
}

// ═══════════════════════════════════════════════════════════════════
// IsActive Predicate
// ═══════════════════════════════════════════════════════════════════

void test_is_active_predicate() {
    InputBinding binding(bus, fakeTime.provider());
    bool isActive = false;

    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.scopeId = 1;  // Must have scope for predicate to be checked
    btn.isActive = [&]() { return isActive; };
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    // Not active - no trigger
    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(0, actionCount);

    // Now active
    isActive = true;
    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(1, actionCount);
}

// ═══════════════════════════════════════════════════════════════════
// Enable/Disable
// ═══════════════════════════════════════════════════════════════════

void test_set_bindings_enabled() {
    InputBinding binding(bus, fakeTime.provider());

    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    binding.setBindingsEnabled(false);
    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(0, actionCount);

    binding.setBindingsEnabled(true);
    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(1, actionCount);
}

// ═══════════════════════════════════════════════════════════════════
// Button State
// ═══════════════════════════════════════════════════════════════════

void test_is_button_pressed() {
    InputBinding binding(bus, fakeTime.provider());

    TEST_ASSERT_FALSE(binding.isButtonPressed(1));

    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_TRUE(binding.isButtonPressed(1));

    bus.emit(ButtonReleaseEvent{1});
    TEST_ASSERT_FALSE(binding.isButtonPressed(1));
}

// ═══════════════════════════════════════════════════════════════════
// Latch State
// ═══════════════════════════════════════════════════════════════════

void test_latch_state() {
    InputBinding binding(bus, fakeTime.provider());

    TEST_ASSERT_FALSE(binding.isLatched(1));

    binding.setLatch(1, true);
    TEST_ASSERT_TRUE(binding.isLatched(1));

    binding.setLatch(1, false);
    TEST_ASSERT_FALSE(binding.isLatched(1));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Basic registration
    RUN_TEST(test_register_button_binding_returns_id);
    RUN_TEST(test_register_encoder_binding_returns_id);
    RUN_TEST(test_sequential_ids);

    // Button press
    RUN_TEST(test_button_press_triggers_action);
    RUN_TEST(test_button_press_wrong_id_no_trigger);
    RUN_TEST(test_button_release_triggers_action);

    // Long press
    RUN_TEST(test_long_press_triggers_after_duration);
    RUN_TEST(test_long_press_only_triggers_once);
    RUN_TEST(test_long_press_custom_duration);

    // Double tap
    RUN_TEST(test_double_tap_triggers);
    RUN_TEST(test_double_tap_outside_window_no_trigger);

    // Encoder
    RUN_TEST(test_encoder_turn_triggers_action);
    RUN_TEST(test_encoder_turn_while_pressed);

    // Remove
    RUN_TEST(test_remove_by_id);
    RUN_TEST(test_remove_invalid_id);

    // Clear
    RUN_TEST(test_clear_all_bindings);
    RUN_TEST(test_clear_button_bindings_only);

    // Scopes
    RUN_TEST(test_scoped_binding_priority);
    RUN_TEST(test_clear_scope);

    // Predicate
    RUN_TEST(test_is_active_predicate);

    // Enable/disable
    RUN_TEST(test_set_bindings_enabled);

    // State
    RUN_TEST(test_is_button_pressed);
    RUN_TEST(test_latch_state);

    UNITY_END();
    return 0;
}
