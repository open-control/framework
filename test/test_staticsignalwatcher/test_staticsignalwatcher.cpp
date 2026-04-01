#include <unity.h>

#include <oc/state/NotificationQueue.hpp>
#include <oc/state/Signal.hpp>
#include <oc/state/SignalString.hpp>
#include <oc/state/SignalVector.hpp>
#include <oc/state/StaticSignalWatcher.hpp>

using namespace oc::state;

namespace {

struct TestOwner {
    int gridCount = 0;
    int overlayCount = 0;

    void onGridChanged() { gridCount++; }
    void onOverlayChanged() { overlayCount++; }
};

}  // namespace

void setUp() {
    NotificationQueue::instance().flush();
    NotificationQueue::instance().setDeferredMode(false);
}

void tearDown() {
    NotificationQueue::instance().flush();
    NotificationQueue::instance().setDeferredMode(true);
}

void test_static_watch_group_coalesces_multiple_signals() {
    TestOwner owner{};
    Signal<int, 4> a{0};
    Signal<int, 4> b{0};
    StaticWatchGroup<4> group;

    group.bind<&TestOwner::onGridChanged>(owner, 0, "test.grid");
    TEST_ASSERT_TRUE(group.watchAll(a, b));

    NotificationQueue::instance().setDeferredMode(true);
    a.set(1);
    b.set(2);

    TEST_ASSERT_EQUAL(0, owner.gridCount);
    NotificationQueue::instance().flush();
    TEST_ASSERT_EQUAL(1, owner.gridCount);
}

void test_static_watch_group_rejects_capacity_overflow() {
    TestOwner owner{};
    Signal<int, 4> a{0};
    Signal<int, 4> b{0};
    StaticWatchGroup<1> group;

    group.bind<&TestOwner::onGridChanged>(owner, 0, "test.grid");

    TEST_ASSERT_TRUE(group.watch(a));
    TEST_ASSERT_FALSE(group.watch(b));
    TEST_ASSERT_EQUAL(1, group.subscriptionCount());
}

void test_static_signal_watcher_supports_multiple_groups() {
    TestOwner owner{};
    Signal<int, 4> a{0};
    SignalString name{"Init"};
    SignalVector<int, 4> values;
    StaticSignalWatcher<2, 4> watcher;

    TEST_ASSERT_TRUE(watcher.watchAll<&TestOwner::onGridChanged>(owner, "grid", a, values));
    TEST_ASSERT_TRUE(watcher.watchAll<&TestOwner::onOverlayChanged>(owner, "overlay", name));

    NotificationQueue::instance().setDeferredMode(true);
    a.set(1);
    values.set({1, 2});
    name.set("Updated");

    NotificationQueue::instance().flush();

    TEST_ASSERT_EQUAL(1, owner.gridCount);
    TEST_ASSERT_EQUAL(1, owner.overlayCount);
    TEST_ASSERT_EQUAL(2, watcher.groupCount());
    TEST_ASSERT_EQUAL(3, watcher.subscriptionCount());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_static_watch_group_coalesces_multiple_signals);
    RUN_TEST(test_static_watch_group_rejects_capacity_overflow);
    RUN_TEST(test_static_signal_watcher_supports_multiple_groups);
    return UNITY_END();
}
