#include <unity.h>

#include <oc/state/ExclusiveVisibilityStack.hpp>
#include <oc/state/Signal.hpp>

using namespace oc::state;

namespace {

enum class Overlay : uint8_t {
    NONE = 0,
    A,
    B,
    COUNT
};

}  // namespace

void setUp() {}
void tearDown() {}

void test_cleanup_handle_clears_callback_on_destruction() {
    ExclusiveVisibilityStack<Overlay> stack;

    Signal<bool> a{false};
    Signal<bool> b{false};
    stack.registerItem(Overlay::A, a);
    stack.registerItem(Overlay::B, b);

    bool called = false;
    {
        auto handle = stack.setCleanupCallbackScoped([&](Overlay) { called = true; });

        stack.show(Overlay::A);
        TEST_ASSERT_TRUE(a.get());

        stack.hide();
        TEST_ASSERT_TRUE(called);
    }

    called = false;
    stack.show(Overlay::A);
    stack.hide();
    TEST_ASSERT_FALSE(called);
}

void test_cleanup_handle_does_not_clear_replaced_callback() {
    ExclusiveVisibilityStack<Overlay> stack;

    Signal<bool> a{false};
    stack.registerItem(Overlay::A, a);

    bool called1 = false;
    bool called2 = false;

    {
        auto handle = stack.setCleanupCallbackScoped([&](Overlay) { called1 = true; });

        // Replace callback; the handle must not clear this newer one.
        stack.setCleanupCallback([&](Overlay) { called2 = true; });
    }

    stack.show(Overlay::A);
    stack.hide();

    TEST_ASSERT_FALSE(called1);
    TEST_ASSERT_TRUE(called2);
}

void test_register_item_accepts_custom_capacity_signal() {
    ExclusiveVisibilityStack<Overlay> stack;

    Signal<bool, 8> a{false};
    Signal<bool, 8> b{false};
    stack.registerItem(Overlay::A, a);
    stack.registerItem(Overlay::B, b);

    stack.show(Overlay::A);
    TEST_ASSERT_TRUE(a.get());
    TEST_ASSERT_FALSE(b.get());

    stack.show(Overlay::B);
    TEST_ASSERT_FALSE(a.get());
    TEST_ASSERT_TRUE(b.get());

    stack.hideAll();
    TEST_ASSERT_FALSE(a.get());
    TEST_ASSERT_FALSE(b.get());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_cleanup_handle_clears_callback_on_destruction);
    RUN_TEST(test_cleanup_handle_does_not_clear_replaced_callback);
    RUN_TEST(test_register_item_accepts_custom_capacity_signal);
    UNITY_END();
    return 0;
}
