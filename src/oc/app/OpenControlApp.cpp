#include "OpenControlApp.hpp"

#include <config/PlatformCompat.hpp>
#include <oc/Config.hpp>
#include <oc/core/event/Events.hpp>
#include <oc/diagnostics/Performance.hpp>
#include <oc/log/Log.hpp>
#include <oc/state/NotificationQueue.hpp>

namespace oc::app {

namespace {
constexpr uint32_t MIDI_PRE_CONTEXT_DRAIN_BUDGET_US = 500;
constexpr uint32_t MIDI_POST_CONTEXT_DRAIN_BUDGET_US = 500;
}  // namespace

OpenControlApp::~OpenControlApp() = default;

OpenControlApp::OpenControlApp(OpenControlApp&&) noexcept = default;
OpenControlApp& OpenControlApp::operator=(OpenControlApp&&) noexcept = default;

bool OpenControlApp::registerPreContextUpdateHook(oc::type::ActionCallback callback) {
    if (!callback) {
        return false;
    }

    for (auto& slot : pre_context_update_hooks_) {
        if (!slot) {
            slot = std::move(callback);
            return true;
        }
    }

    OC_LOG_WARN("{}", "[OpenControlApp] pre-context update hook registry full");
    return false;
}

FLASHMEM void OpenControlApp::begin() {
    // Helper: check result and halt on error
    auto check = [](const oc::type::Result<void>& result, const char* component) {
        if (!result) {
            OC_LOG_ERROR("{} init failed: {}", component, oc::type::errorCodeToString(result.error().code));
            while (true) {}  // Halt - no recovery in embedded
        }
    };

    // Initialize hardware components
    if (display_) check(display_->init(), "Display");
    if (midi_) check(midi_->init(), "MIDI");
    if (frames_) check(frames_->init(), "Serial");
    if (encoders_) check(encoders_->init(), "Encoders");
    if (buttons_) check(buttons_->init(), "Buttons");

    // Wire HAL callbacks to EventBus
    if (buttons_) {
        buttons_->setCallback([this](oc::type::ButtonID id, oc::type::ButtonEvent evt) {
            if (evt == oc::type::ButtonEvent::PRESSED) {
                event_bus_.emit(core::event::ButtonPressEvent(id, true));
            } else {
                event_bus_.emit(core::event::ButtonReleaseEvent(id));
            }
        });
    }

    if (encoders_) {
        encoders_->setCallback([this](oc::type::EncoderID id, float value) {
            event_bus_.emit(core::event::EncoderChangedEvent(id, value));
        });
    }

    if (midi_) {
        midi_->setOnCC([this](uint8_t ch, uint8_t cc, uint8_t val) {
            event_bus_.emit(core::event::MidiCCEvent(ch, cc, val));
        });
        midi_->setOnNoteOn([this](uint8_t ch, uint8_t note, uint8_t vel) {
            event_bus_.emit(core::event::MidiNoteOnEvent(ch, note, vel));
        });
        midi_->setOnNoteOff([this](uint8_t ch, uint8_t note, uint8_t vel) {
            event_bus_.emit(core::event::MidiNoteOffEvent(ch, note, vel));
        });
        midi_->setOnSysEx([this](const uint8_t* data, size_t len) {
            event_bus_.emit(core::event::SysExEvent(data, static_cast<uint16_t>(len)));
        });
        midi_->setOnClock([this](uint64_t timestampUs) {
            event_bus_.emit(core::event::MidiClockEvent(timestampUs));
        });
        midi_->setOnStart([this]() {
            event_bus_.emit(core::event::MidiStartEvent());
        });
        midi_->setOnContinue([this]() {
            event_bus_.emit(core::event::MidiContinueEvent());
        });
        midi_->setOnStop([this]() {
            event_bus_.emit(core::event::MidiStopEvent());
        });
    }

    check(contexts_->begin(), "Context");
}

void OpenControlApp::update() {
    uint32_t now = time_provider_();

    {
        OC_PERF_SCOPE(perfInput, "app.input");
        // Process tick before hardware updates so time-dependent input state is current.
        if (input_binding_) input_binding_->processTick();
        if (midi_) midi_->pollInput();
        if (frames_) frames_->update();
        if (encoders_) encoders_->update();
        if (buttons_) buttons_->update(now);
    }

    {
        OC_PERF_SCOPE(perfHooks, "app.pre-context-hooks");
        for (auto& hook : pre_context_update_hooks_) {
            if (hook) hook();
        }
    }

    {
        OC_PERF_SCOPE(perfMidiPre, "app.midi-pre-drain");
        if (midi_) midi_->serviceOutput(MIDI_PRE_CONTEXT_DRAIN_BUDGET_US);
    }

    {
        OC_PERF_SCOPE(perfContext, "app.context");
        contexts_->update();
    }

    {
        OC_PERF_SCOPE(perfMidiPost, "app.midi-post-drain");
        if (midi_) midi_->serviceOutput(MIDI_POST_CONTEXT_DRAIN_BUDGET_US);
    }

    {
        OC_PERF_SCOPE(perfNotifications, "app.notifications");
        state::NotificationQueue::instance().flush();
    }

}

}  // namespace oc::app
