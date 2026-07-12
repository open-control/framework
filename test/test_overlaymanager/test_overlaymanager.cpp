#include <unity.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <oc/context/OverlayManager.hpp>
#include <oc/state/ExclusiveVisibilityStack.hpp>
#include <oc/state/Signal.hpp>

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

}  // namespace

void setUp() {}
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

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_presentation_follows_replacement_and_stacked_hide);
    return UNITY_END();
}
