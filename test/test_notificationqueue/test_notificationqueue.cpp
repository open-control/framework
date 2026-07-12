#include <unity.h>

#include <oc/state/NotificationQueue.hpp>

using namespace oc::state;

namespace {

struct CounterContext {
    int count = 0;
    NotificationQueue::Key reenqueueKey{nullptr, 0};
    bool reenqueueTriggered = false;
};

struct CancelContext {
    int count = 0;
    void* owner = nullptr;
};

void incrementCounter(void* context, size_t) {
    auto* counter = static_cast<int*>(context);
    (*counter)++;
}

void incrementContextCounter(void* context, size_t) {
    auto* counter = static_cast<CounterContext*>(context);
    counter->count++;
}

void incrementAndReenqueue(void* context, size_t) {
    auto* counter = static_cast<CounterContext*>(context);
    counter->count++;
    if (!counter->reenqueueTriggered) {
        counter->reenqueueTriggered = true;
        NotificationQueue::instance().enqueue(counter->reenqueueKey, context, incrementContextCounter);
    }
}

void incrementAndCancelOwner(void* context, size_t) {
    auto* cancel = static_cast<CancelContext*>(context);
    ++cancel->count;
    NotificationQueue::instance().cancelOwner(cancel->owner);
}

}  // namespace

void setUp() {
    NotificationQueue::instance().flush();
    NotificationQueue::instance().setDeferredMode(true);
    NotificationQueue::instance().resetOverflowCount();
}

void tearDown() {
    NotificationQueue::instance().flush();
    NotificationQueue::instance().setDeferredMode(true);
}

void test_enqueue_coalesces_same_key_without_dynamic_storage() {
    int counter = 0;
    auto key = NotificationQueue::Key(&counter, 1);

    NotificationQueue::instance().enqueue(key, &counter, incrementCounter);
    NotificationQueue::instance().enqueue(key, &counter, incrementCounter);

    TEST_ASSERT_EQUAL(1, NotificationQueue::instance().pendingCount());
    NotificationQueue::instance().flush();
    TEST_ASSERT_EQUAL(1, counter);
}

void test_flush_processes_notifications_enqueued_during_flush() {
    CounterContext context{};
    context.reenqueueKey = NotificationQueue::Key(&context, 2);

    NotificationQueue::instance().enqueue({&context, 1}, &context, incrementAndReenqueue);

    NotificationQueue::instance().flush();

    TEST_ASSERT_EQUAL(2, context.count);
    TEST_ASSERT_FALSE(NotificationQueue::instance().hasPending());
}

void test_overflow_is_bounded_and_counted() {
    int counters[NotificationQueue::maxPending() + 1] = {};

    for (size_t i = 0; i < NotificationQueue::maxPending(); ++i) {
        NotificationQueue::instance().enqueue({&counters[i], i}, &counters[i], incrementCounter);
    }

    TEST_ASSERT_EQUAL(NotificationQueue::maxPending(), NotificationQueue::instance().pendingCount());
    TEST_ASSERT_FALSE(NotificationQueue::instance().hasOverflowed());

    NotificationQueue::instance().enqueue({&counters[NotificationQueue::maxPending()], 99},
                                          &counters[NotificationQueue::maxPending()],
                                          incrementCounter);

    TEST_ASSERT_TRUE(NotificationQueue::instance().hasOverflowed());
    TEST_ASSERT_EQUAL(1, NotificationQueue::instance().overflowCount());
    TEST_ASSERT_EQUAL(NotificationQueue::maxPending(), NotificationQueue::instance().pendingCount());

    NotificationQueue::instance().flush();
    TEST_ASSERT_FALSE(NotificationQueue::instance().hasPending());
}

void test_cancel_removes_only_matching_key() {
    int counters[2] = {};
    void* owner = &counters;
    NotificationQueue::instance().enqueue({owner, 1}, &counters[0], incrementCounter);
    NotificationQueue::instance().enqueue({owner, 2}, &counters[1], incrementCounter);

    NotificationQueue::instance().cancel({owner, 1});
    TEST_ASSERT_EQUAL(1, NotificationQueue::instance().pendingCount());
    NotificationQueue::instance().flush();

    TEST_ASSERT_EQUAL(0, counters[0]);
    TEST_ASSERT_EQUAL(1, counters[1]);
}

void test_cancel_owner_suppresses_later_callbacks_in_active_wave() {
    CancelContext context{};
    context.owner = &context;
    int later = 0;
    NotificationQueue::instance().enqueue(
        {context.owner, 1},
        &context,
        incrementAndCancelOwner
    );
    NotificationQueue::instance().enqueue({context.owner, 2}, &later, incrementCounter);

    NotificationQueue::instance().flush();

    TEST_ASSERT_EQUAL(1, context.count);
    TEST_ASSERT_EQUAL(0, later);
    TEST_ASSERT_FALSE(NotificationQueue::instance().hasPending());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_enqueue_coalesces_same_key_without_dynamic_storage);
    RUN_TEST(test_flush_processes_notifications_enqueued_during_flush);
    RUN_TEST(test_overflow_is_bounded_and_counted);
    RUN_TEST(test_cancel_removes_only_matching_key);
    RUN_TEST(test_cancel_owner_suppresses_later_callbacks_in_active_wave);
    return UNITY_END();
}
