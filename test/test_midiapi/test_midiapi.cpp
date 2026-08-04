#include <memory>
#include <utility>

#include <unity.h>

#include <oc/api/MidiAPI.hpp>
#include <oc/app/AppBuilder.hpp>
#include <oc/app/OpenControlApp.hpp>
#include <oc/interface/IMidi.hpp>

namespace {

using oc::interface::MidiOutputAcceptance;

uint32_t testTimeMs() {
    return 123U;
}

class MockMidi final : public oc::interface::IMidi {
public:
    oc::type::Result<void> init() override {
        return oc::type::Result<void>::ok();
    }

    void update() override {}

    void pollInput() override {
        ++pollCount;
    }

    void serviceOutput() override {
        ++unboundedServiceCount;
    }

    void serviceOutput(uint32_t budgetUs) override {
        ++boundedServiceCount;
        lastServiceBudgetUs = budgetUs;
    }

    MidiOutputAcceptance sendCC(uint8_t, uint8_t, uint8_t) override {
        ++sendCount;
        return nextAcceptance;
    }
    MidiOutputAcceptance sendNoteOn(uint8_t, uint8_t, uint8_t) override {
        ++sendCount;
        return nextAcceptance;
    }
    MidiOutputAcceptance sendNoteOff(uint8_t, uint8_t, uint8_t) override {
        ++sendCount;
        return nextAcceptance;
    }
    MidiOutputAcceptance sendSysEx(const uint8_t*, size_t) override {
        ++sendCount;
        return nextAcceptance;
    }
    MidiOutputAcceptance sendProgramChange(uint8_t, uint8_t) override {
        ++sendCount;
        return nextAcceptance;
    }
    MidiOutputAcceptance sendPitchBend(uint8_t, int16_t) override {
        ++sendCount;
        return nextAcceptance;
    }
    MidiOutputAcceptance sendChannelPressure(uint8_t, uint8_t) override {
        ++sendCount;
        return nextAcceptance;
    }
    MidiOutputAcceptance sendClock() override {
        ++sendCount;
        return nextAcceptance;
    }
    MidiOutputAcceptance sendStart() override {
        ++sendCount;
        return nextAcceptance;
    }
    MidiOutputAcceptance sendStop() override {
        ++sendCount;
        return nextAcceptance;
    }
    MidiOutputAcceptance sendContinue() override {
        ++sendCount;
        return nextAcceptance;
    }

    void setOnCC(CCCallback cb) override { onCC = std::move(cb); }
    void setOnNoteOn(NoteCallback cb) override { onNoteOn = std::move(cb); }
    void setOnNoteOff(NoteCallback cb) override { onNoteOff = std::move(cb); }
    void setOnSysEx(SysExCallback cb) override { onSysEx = std::move(cb); }
    void setOnClock(ClockCallback cb) override { onClock = std::move(cb); }
    void setOnStart(RealtimeCallback cb) override { onStart = std::move(cb); }
    void setOnStop(RealtimeCallback cb) override { onStop = std::move(cb); }
    void setOnContinue(RealtimeCallback cb) override { onContinue = std::move(cb); }

    MidiOutputAcceptance nextAcceptance = MidiOutputAcceptance::ACCEPTED;
    uint32_t sendCount = 0;
    uint32_t pollCount = 0;
    uint32_t boundedServiceCount = 0;
    uint32_t unboundedServiceCount = 0;
    uint32_t lastServiceBudgetUs = 0;
    CCCallback onCC;
    NoteCallback onNoteOn;
    NoteCallback onNoteOff;
    SysExCallback onSysEx;
    ClockCallback onClock;
    RealtimeCallback onStart;
    RealtimeCallback onStop;
    RealtimeCallback onContinue;
};

void assertAcceptance(MidiOutputAcceptance expected,
                      MidiOutputAcceptance actual) {
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(expected),
        static_cast<uint8_t>(actual)
    );
}

void test_midi_api_propagates_transport_ownership() {
    MockMidi transport;
    oc::api::MidiAPI midi{transport};

    transport.nextAcceptance = MidiOutputAcceptance::REJECTED;
    assertAcceptance(
        MidiOutputAcceptance::REJECTED,
        midi.sendNoteOn(0U, 60U, 100U)
    );
    TEST_ASSERT_EQUAL_UINT32(1U, transport.sendCount);

    transport.nextAcceptance = MidiOutputAcceptance::ACCEPTED;
    assertAcceptance(
        MidiOutputAcceptance::ACCEPTED,
        midi.sendClock()
    );
    TEST_ASSERT_EQUAL_UINT32(2U, transport.sendCount);
}

void test_midi_api_validation_rejects_before_transport() {
    MockMidi transport;
    oc::api::MidiAPI midi{transport};

    assertAcceptance(
        MidiOutputAcceptance::REJECTED,
        midi.sendCC(16U, 1U, 2U)
    );
    assertAcceptance(
        MidiOutputAcceptance::REJECTED,
        midi.sendNoteOff(0U, 128U, 0U)
    );
    assertAcceptance(
        MidiOutputAcceptance::REJECTED,
        midi.sendSysEx(nullptr, 0U)
    );
    assertAcceptance(
        MidiOutputAcceptance::REJECTED,
        midi.sendPitchBend(0U, 8192)
    );
    TEST_ASSERT_EQUAL_UINT32(0U, transport.sendCount);
}

void test_app_update_owns_the_bounded_foreground_drains() {
    auto transport = std::make_unique<MockMidi>();
    auto* observed = transport.get();
    auto app = oc::app::AppBuilder()
        .timeProvider(testTimeMs)
        .midi(std::move(transport))
        .build();

    app.update();

    TEST_ASSERT_EQUAL_UINT32(1U, observed->pollCount);
    TEST_ASSERT_EQUAL_UINT32(2U, observed->boundedServiceCount);
    TEST_ASSERT_EQUAL_UINT32(0U, observed->unboundedServiceCount);
    TEST_ASSERT_EQUAL_UINT32(500U, observed->lastServiceBudgetUs);
}

}  // namespace

void setUp() {}
void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_midi_api_propagates_transport_ownership);
    RUN_TEST(test_midi_api_validation_rejects_before_transport);
    RUN_TEST(test_app_update_owns_the_bounded_foreground_drains);
    return UNITY_END();
}
