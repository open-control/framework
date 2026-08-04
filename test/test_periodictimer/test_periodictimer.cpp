#include <unity.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <oc/realtime/PeriodicTimer.hpp>

namespace {

constexpr std::size_t CHANNEL_COUNT = 4;
constexpr std::size_t CONTESTED_CHANNEL = CHANNEL_COUNT - 1;
constexpr int PRIMARY_CALLBACK = 11;
constexpr int COMPETING_CALLBACK = 22;

enum class MutationPoint : uint8_t {
    None,
    BeginScanSelected,
    BeginCallbackPublished,
    BeginChannelEnabled,
    EndCallbackCleared,
    EndChannelDisabled,
    EndOwnerReleased,
};

enum class Operation : uint8_t {
    Begin,
    End,
};

struct ChannelState {
    bool enabled = false;
    int callback = 0;
};

class FakeIntervalTimer;
class FakePeriodicTimer;

std::array<ChannelState, CHANNEL_COUNT> channels{};
std::array<FakeIntervalTimer*, 2> timerInstances{};
std::size_t timerInstanceCount = 0;

uint32_t primask = 0;
MutationPoint injectionPoint = MutationPoint::None;
Operation pendingOperation = Operation::Begin;
FakePeriodicTimer* competingTimer = nullptr;
bool interruptAttempted = false;
bool interruptRanInsideTransition = false;
bool interruptDeferred = false;
bool pendingServiced = false;
bool pendingBeginResult = false;
bool everyMutationMasked = true;
bool vendorDestructorSawOwnedChannel = false;
uint32_t lastBeginEntryMask = 0;
uint32_t lastEndEntryMask = 0;

bool servicePendingInterrupt();

void visitMutationPoint(MutationPoint point) {
    everyMutationMasked = everyMutationMasked && (primask == 1U);
    if (point != injectionPoint || interruptAttempted) {
        return;
    }

    interruptAttempted = true;
    if (primask == 0U) {
        interruptRanInsideTransition = true;
        (void)servicePendingInterrupt();
    } else {
        interruptDeferred = true;
    }
}

class FakeInterruptGuard {
public:
    FakeInterruptGuard()
        : savedPrimask_(primask) {
        primask = 1U;
    }

    ~FakeInterruptGuard() {
        primask = savedPrimask_;
    }

    FakeInterruptGuard(const FakeInterruptGuard&) = delete;
    FakeInterruptGuard& operator=(const FakeInterruptGuard&) = delete;

private:
    uint32_t savedPrimask_ = 0;
};

class FakeIntervalTimer {
public:
    FakeIntervalTimer() {
        TEST_ASSERT_LESS_THAN_UINT32(timerInstances.size(), timerInstanceCount);
        timerInstances[timerInstanceCount++] = this;
    }

    ~FakeIntervalTimer() {
        if (channel_ >= 0) {
            vendorDestructorSawOwnedChannel = true;
            end();
        }
    }

    bool begin(int callback, uint32_t periodUs) {
        (void)periodUs;
        lastBeginEntryMask = primask;

        if (channel_ < 0) {
            for (std::size_t index = 0; index < channels.size(); ++index) {
                if (!channels[index].enabled) {
                    channel_ = static_cast<int>(index);
                    break;
                }
            }
            if (channel_ < 0) {
                return false;
            }
            visitMutationPoint(MutationPoint::BeginScanSelected);
        }

        auto& channel = channels[static_cast<std::size_t>(channel_)];
        channel.callback = callback;
        visitMutationPoint(MutationPoint::BeginCallbackPublished);
        channel.enabled = true;
        visitMutationPoint(MutationPoint::BeginChannelEnabled);
        return true;
    }

    void end() {
        lastEndEntryMask = primask;
        if (channel_ < 0) {
            return;
        }

        auto& channel = channels[static_cast<std::size_t>(channel_)];
        channel.callback = 0;
        visitMutationPoint(MutationPoint::EndCallbackCleared);
        channel.enabled = false;
        visitMutationPoint(MutationPoint::EndChannelDisabled);
        channel_ = -1;
        visitMutationPoint(MutationPoint::EndOwnerReleased);
    }

    void priority(uint8_t value) {
        (void)value;
    }

    bool owns(std::size_t channel) const {
        return channel_ == static_cast<int>(channel);
    }

private:
    int channel_ = -1;
};

class FakePeriodicTimer {
public:
    ~FakePeriodicTimer() {
        end();
    }

    bool begin(int callback, uint32_t periodUs) {
        return oc::realtime::detail::guardedTimerBegin<FakeInterruptGuard>(
            timer_,
            callback,
            periodUs
        );
    }

    void end() {
        oc::realtime::detail::guardedTimerEnd<FakeInterruptGuard>(timer_);
    }

private:
    FakeIntervalTimer timer_{};
};

void resetModel() {
    channels = {};
    for (std::size_t index = 0; index < CONTESTED_CHANNEL; ++index) {
        channels[index].enabled = true;
        channels[index].callback = 100 + static_cast<int>(index);
    }
    timerInstances = {};
    timerInstanceCount = 0;
    primask = 0;
    injectionPoint = MutationPoint::None;
    pendingOperation = Operation::Begin;
    competingTimer = nullptr;
    interruptAttempted = false;
    interruptRanInsideTransition = false;
    interruptDeferred = false;
    pendingServiced = false;
    pendingBeginResult = false;
    everyMutationMasked = true;
    vendorDestructorSawOwnedChannel = false;
    lastBeginEntryMask = 0;
    lastEndEntryMask = 0;
}

bool servicePendingInterrupt() {
    if ((!interruptDeferred && !interruptRanInsideTransition) ||
        pendingServiced || primask != 0U || competingTimer == nullptr) {
        return false;
    }

    pendingServiced = true;
    interruptDeferred = false;
    injectionPoint = MutationPoint::None;
    if (pendingOperation == Operation::Begin) {
        pendingBeginResult = competingTimer->begin(COMPETING_CALLBACK, 1000U);
    } else {
        competingTimer->end();
    }
    return true;
}

std::size_t contestedOwnerCount() {
    std::size_t count = 0;
    for (std::size_t index = 0; index < timerInstanceCount; ++index) {
        if (timerInstances[index]->owns(CONTESTED_CHANNEL)) {
            ++count;
        }
    }
    return count;
}

void assertSentinelsIntact(std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) {
        TEST_ASSERT_TRUE(channels[index].enabled);
        TEST_ASSERT_EQUAL_INT(100 + static_cast<int>(index), channels[index].callback);
    }
}

void runInterleaving(Operation outerOperation,
                     Operation nestedOperation,
                     MutationPoint point,
                     uint32_t incomingPrimask) {
    resetModel();

    {
        FakePeriodicTimer primary;
        FakePeriodicTimer competitor;
        competingTimer = &competitor;

        std::size_t fixedSentinelCount = CONTESTED_CHANNEL;
        if (nestedOperation == Operation::End) {
            // Give the competitor a real live channel so its pending end is a
            // shared-pool release rather than a no-op.
            fixedSentinelCount = CONTESTED_CHANNEL - 1U;
            channels[fixedSentinelCount] = {};
            TEST_ASSERT_TRUE(competitor.begin(COMPETING_CALLBACK, 1000U));
        }

        if (outerOperation == Operation::End) {
            TEST_ASSERT_TRUE(primary.begin(PRIMARY_CALLBACK, 1000U));
        }

        pendingOperation = nestedOperation;
        injectionPoint = point;
        primask = incomingPrimask;

        if (outerOperation == Operation::Begin) {
            TEST_ASSERT_TRUE(primary.begin(PRIMARY_CALLBACK, 1000U));
        } else {
            primary.end();
        }

        TEST_ASSERT_TRUE(interruptAttempted);
        TEST_ASSERT_TRUE(interruptDeferred);
        TEST_ASSERT_FALSE(interruptRanInsideTransition);
        TEST_ASSERT_FALSE(pendingServiced);
        TEST_ASSERT_TRUE(everyMutationMasked);
        TEST_ASSERT_EQUAL_UINT32(incomingPrimask, primask);
        assertSentinelsIntact(fixedSentinelCount);
        if (nestedOperation == Operation::End) {
            TEST_ASSERT_TRUE(channels[CONTESTED_CHANNEL - 1U].enabled);
            TEST_ASSERT_EQUAL_INT(
                COMPETING_CALLBACK,
                channels[CONTESTED_CHANNEL - 1U].callback
            );
        }

        if (outerOperation == Operation::Begin) {
            TEST_ASSERT_TRUE(channels[CONTESTED_CHANNEL].enabled);
            TEST_ASSERT_EQUAL_INT(PRIMARY_CALLBACK, channels[CONTESTED_CHANNEL].callback);
            TEST_ASSERT_EQUAL_UINT32(1U, contestedOwnerCount());
        } else {
            TEST_ASSERT_FALSE(channels[CONTESTED_CHANNEL].enabled);
            TEST_ASSERT_EQUAL_INT(0, channels[CONTESTED_CHANNEL].callback);
            TEST_ASSERT_EQUAL_UINT32(0U, contestedOwnerCount());
        }

        if (incomingPrimask != 0U) {
            TEST_ASSERT_FALSE(servicePendingInterrupt());
            primask = 0U;
        }
        TEST_ASSERT_TRUE(servicePendingInterrupt());
        TEST_ASSERT_TRUE(pendingServiced);
        TEST_ASSERT_EQUAL_UINT32(0U, primask);
        TEST_ASSERT_TRUE(everyMutationMasked);
        assertSentinelsIntact(fixedSentinelCount);
        if (nestedOperation == Operation::End) {
            TEST_ASSERT_FALSE(channels[CONTESTED_CHANNEL - 1U].enabled);
            TEST_ASSERT_EQUAL_INT(0, channels[CONTESTED_CHANNEL - 1U].callback);
        }

        if (outerOperation == Operation::Begin) {
            TEST_ASSERT_TRUE(channels[CONTESTED_CHANNEL].enabled);
            TEST_ASSERT_EQUAL_INT(PRIMARY_CALLBACK, channels[CONTESTED_CHANNEL].callback);
            TEST_ASSERT_EQUAL_UINT32(1U, contestedOwnerCount());
            if (nestedOperation == Operation::Begin) {
                TEST_ASSERT_FALSE(pendingBeginResult);
            }
        } else if (nestedOperation == Operation::Begin) {
            TEST_ASSERT_TRUE(pendingBeginResult);
            TEST_ASSERT_TRUE(channels[CONTESTED_CHANNEL].enabled);
            TEST_ASSERT_EQUAL_INT(COMPETING_CALLBACK, channels[CONTESTED_CHANNEL].callback);
            TEST_ASSERT_EQUAL_UINT32(1U, contestedOwnerCount());
        } else {
            TEST_ASSERT_FALSE(channels[CONTESTED_CHANNEL].enabled);
            TEST_ASSERT_EQUAL_INT(0, channels[CONTESTED_CHANNEL].callback);
            TEST_ASSERT_EQUAL_UINT32(0U, contestedOwnerCount());
        }
    }

    TEST_ASSERT_FALSE(vendorDestructorSawOwnedChannel);
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_all_begin_end_interleavings_are_indivisible() {
    constexpr std::array beginPoints{
        MutationPoint::BeginScanSelected,
        MutationPoint::BeginCallbackPublished,
        MutationPoint::BeginChannelEnabled,
    };
    constexpr std::array endPoints{
        MutationPoint::EndCallbackCleared,
        MutationPoint::EndChannelDisabled,
        MutationPoint::EndOwnerReleased,
    };
    constexpr std::array operations{Operation::Begin, Operation::End};
    constexpr std::array<uint32_t, 2> incomingMasks{0U, 1U};

    std::size_t caseCount = 0;
    for (const auto nested : operations) {
        for (const auto point : beginPoints) {
            for (const auto mask : incomingMasks) {
                runInterleaving(Operation::Begin, nested, point, mask);
                ++caseCount;
            }
        }
        for (const auto point : endPoints) {
            for (const auto mask : incomingMasks) {
                runInterleaving(Operation::End, nested, point, mask);
                ++caseCount;
            }
        }
    }

    TEST_ASSERT_EQUAL_UINT32(24U, caseCount);
}

void test_failure_and_repeated_end_restore_incoming_primask() {
    resetModel();
    channels[CONTESTED_CHANNEL].enabled = true;
    channels[CONTESTED_CHANNEL].callback = 103;

    {
        FakePeriodicTimer timer;
        primask = 0U;
        TEST_ASSERT_FALSE(timer.begin(PRIMARY_CALLBACK, 1000U));
        TEST_ASSERT_EQUAL_UINT32(1U, lastBeginEntryMask);
        TEST_ASSERT_EQUAL_UINT32(0U, primask);

        primask = 1U;
        timer.end();
        TEST_ASSERT_EQUAL_UINT32(1U, lastEndEntryMask);
        TEST_ASSERT_EQUAL_UINT32(1U, primask);
        timer.end();
        TEST_ASSERT_EQUAL_UINT32(1U, lastEndEntryMask);
        TEST_ASSERT_EQUAL_UINT32(1U, primask);
    }

    TEST_ASSERT_FALSE(vendorDestructorSawOwnedChannel);
    TEST_ASSERT_TRUE(everyMutationMasked);
}

void test_active_destruction_releases_before_vendor_destructor() {
    resetModel();
    primask = 1U;

    {
        FakePeriodicTimer timer;
        TEST_ASSERT_TRUE(timer.begin(PRIMARY_CALLBACK, 1000U));
        TEST_ASSERT_EQUAL_UINT32(1U, primask);
        TEST_ASSERT_TRUE(channels[CONTESTED_CHANNEL].enabled);
    }

    TEST_ASSERT_EQUAL_UINT32(1U, primask);
    TEST_ASSERT_FALSE(channels[CONTESTED_CHANNEL].enabled);
    TEST_ASSERT_EQUAL_INT(0, channels[CONTESTED_CHANNEL].callback);
    TEST_ASSERT_FALSE(vendorDestructorSawOwnedChannel);
    TEST_ASSERT_TRUE(everyMutationMasked);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_all_begin_end_interleavings_are_indivisible);
    RUN_TEST(test_failure_and_repeated_end_restore_incoming_primask);
    RUN_TEST(test_active_destruction_releases_before_vendor_destructor);
    return UNITY_END();
}
