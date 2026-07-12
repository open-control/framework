#include <unity.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <oc/state/ExclusiveVisibilityStack.hpp>
#include <oc/state/Signal.hpp>

using namespace oc::state;

namespace {

enum class Overlay : uint8_t {
    NONE = 0,
    A,
    B,
    C,
    COUNT
};

struct VisibilityTrace {
    Overlay type = Overlay::NONE;
    bool visible = false;
    uint8_t count = 0;
};

void recordVisibility(void* context, Overlay type, bool visible) {
    auto* trace = static_cast<VisibilityTrace*>(context);
    trace->type = type;
    trace->visible = visible;
    trace->count++;
}

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

void test_replacement_cleans_current_and_stacked_previous() {
    ExclusiveVisibilityStack<Overlay> stack;

    Signal<bool> a{false};
    Signal<bool> b{false};
    Signal<bool> c{false};
    stack.registerItem(Overlay::A, a);
    stack.registerItem(Overlay::B, b);
    stack.registerItem(Overlay::C, c);

    std::array<uint8_t, static_cast<size_t>(Overlay::COUNT)> cleanupCounts{};
    auto cleanup = stack.setCleanupCallbackScoped([&](Overlay type) {
        cleanupCounts[static_cast<size_t>(type)]++;
    });

    stack.show(Overlay::A);
    stack.show(Overlay::B, true);
    stack.show(Overlay::C);

    TEST_ASSERT_FALSE(a.get());
    TEST_ASSERT_FALSE(b.get());
    TEST_ASSERT_TRUE(c.get());
    TEST_ASSERT_EQUAL_UINT8(1, cleanupCounts[static_cast<size_t>(Overlay::A)]);
    TEST_ASSERT_EQUAL_UINT8(1, cleanupCounts[static_cast<size_t>(Overlay::B)]);
    TEST_ASSERT_EQUAL_UINT8(0, cleanupCounts[static_cast<size_t>(Overlay::C)]);
}

void test_second_stacked_show_collapses_oldest_level() {
    ExclusiveVisibilityStack<Overlay> stack;

    Signal<bool> a{false};
    Signal<bool> b{false};
    Signal<bool> c{false};
    stack.registerItem(Overlay::A, a);
    stack.registerItem(Overlay::B, b);
    stack.registerItem(Overlay::C, c);

    stack.show(Overlay::A);
    stack.show(Overlay::B, true);
    stack.show(Overlay::C, true);

    TEST_ASSERT_FALSE(a.get());
    TEST_ASSERT_TRUE(b.get());
    TEST_ASSERT_TRUE(c.get());

    stack.hide();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Overlay::B), static_cast<int>(stack.current()));
    TEST_ASSERT_TRUE(b.get());
    TEST_ASSERT_FALSE(c.get());
}

void test_show_emits_transition_when_signal_was_previously_enabled() {
    ExclusiveVisibilityStack<Overlay> stack;
    Signal<bool> a{true};
    stack.registerItem(Overlay::A, a);

    VisibilityTrace trace{};
    auto transition = stack.setVisibilityTransitionCallbackScoped(&trace, recordVisibility);

    stack.show(Overlay::A);
    TEST_ASSERT_EQUAL_UINT8(1, trace.count);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Overlay::A), static_cast<int>(trace.type));
    TEST_ASSERT_TRUE(trace.visible);

    stack.hide();
    TEST_ASSERT_EQUAL_UINT8(2, trace.count);
    TEST_ASSERT_FALSE(trace.visible);
}

void test_revision_tracks_logical_stack_transitions() {
    ExclusiveVisibilityStack<Overlay> stack;
    Signal<bool> a{false};
    Signal<bool> b{false};
    stack.registerItem(Overlay::A, a);
    stack.registerItem(Overlay::B, b);

    TEST_ASSERT_EQUAL_UINT32(0, stack.revisionSignal().get());
    stack.show(Overlay::A);
    TEST_ASSERT_EQUAL_UINT32(1, stack.revisionSignal().get());
    stack.show(Overlay::B, true);
    TEST_ASSERT_EQUAL_UINT32(2, stack.revisionSignal().get());
    stack.hide();
    TEST_ASSERT_EQUAL_UINT32(3, stack.revisionSignal().get());
    stack.hideAll();
    TEST_ASSERT_EQUAL_UINT32(4, stack.revisionSignal().get());
    stack.hideAll();
    TEST_ASSERT_EQUAL_UINT32(4, stack.revisionSignal().get());
}

void test_repeated_show_of_current_item_is_a_noop() {
    ExclusiveVisibilityStack<Overlay> stack;
    Signal<bool> a{false};
    stack.registerItem(Overlay::A, a);

    VisibilityTrace trace{};
    auto transition = stack.setVisibilityTransitionCallbackScoped(&trace, recordVisibility);

    stack.show(Overlay::A);
    const uint32_t revision = stack.revisionSignal().get();
    stack.show(Overlay::A);

    TEST_ASSERT_EQUAL_UINT8(1, trace.count);
    TEST_ASSERT_EQUAL_UINT32(revision, stack.revisionSignal().get());
    TEST_ASSERT_TRUE(a.get());
}

void test_hide_all_tracks_an_unclaimed_visible_signal() {
    ExclusiveVisibilityStack<Overlay> stack;
    Signal<bool> a{true};
    stack.registerItem(Overlay::A, a);

    stack.hideAll();

    TEST_ASSERT_FALSE(a.get());
    TEST_ASSERT_EQUAL_UINT32(1, stack.revisionSignal().get());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_cleanup_handle_clears_callback_on_destruction);
    RUN_TEST(test_cleanup_handle_does_not_clear_replaced_callback);
    RUN_TEST(test_register_item_accepts_custom_capacity_signal);
    RUN_TEST(test_replacement_cleans_current_and_stacked_previous);
    RUN_TEST(test_second_stacked_show_collapses_oldest_level);
    RUN_TEST(test_show_emits_transition_when_signal_was_previously_enabled);
    RUN_TEST(test_revision_tracks_logical_stack_transitions);
    RUN_TEST(test_repeated_show_of_current_item_is_a_noop);
    RUN_TEST(test_hide_all_tracks_an_unclaimed_visible_signal);
    UNITY_END();
    return 0;
}
