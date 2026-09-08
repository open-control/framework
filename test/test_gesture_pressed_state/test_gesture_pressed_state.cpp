#include <unity.h>
#include <oc/core/input/GestureDetector.hpp>

using namespace oc::core::input;

void setUp() {}
void tearDown() {}

void test_pressed_state_tracks_each_button_and_resets() {
    InputConfig config;
    GestureDetector detector(config);
    TEST_ASSERT_FALSE(detector.hasPressedButtons());
    for (size_t index = 0; index < MAX_BUTTONS; ++index) {
        const auto id = static_cast<oc::type::ButtonID>(index);
        detector.onButtonPress(id, 100);
        TEST_ASSERT_TRUE(detector.hasPressedButtons());
        TEST_ASSERT_TRUE(detector.isPressed(id));
        detector.onButtonRelease(id, 110);
        TEST_ASSERT_FALSE(detector.hasPressedButtons());
        detector.onButtonPress(id, 200);
        detector.resetButton(id);
        TEST_ASSERT_FALSE(detector.isPressed(id));
        TEST_ASSERT_FALSE(detector.hasPressedButtons());
    }
    for (size_t index = 0; index < MAX_BUTTONS; ++index)
        detector.onButtonPress(static_cast<oc::type::ButtonID>(index), 300);
    for (size_t index = 0; index < MAX_BUTTONS; ++index) {
        TEST_ASSERT_TRUE(detector.hasPressedButtons());
        detector.onButtonRelease(static_cast<oc::type::ButtonID>(index), 310);
    }
    TEST_ASSERT_FALSE(detector.hasPressedButtons());
    detector.onButtonPress(0, 400);
    detector.reset();
    TEST_ASSERT_FALSE(detector.hasPressedButtons());
    const auto invalid = static_cast<oc::type::ButtonID>(MAX_BUTTONS);
    detector.onButtonPress(invalid, 500);
    detector.onButtonRelease(invalid, 510);
    detector.resetButton(invalid);
    TEST_ASSERT_FALSE(detector.isPressed(invalid));
    TEST_ASSERT_FALSE(detector.hasPressedButtons());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_pressed_state_tracks_each_button_and_resets);
    return UNITY_END();
}
