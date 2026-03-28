#include "OpenControlApp.hpp"

#include <oc/Config.hpp>
#include <oc/core/event/Events.hpp>
#include <oc/log/Log.hpp>
#include <oc/state/NotificationQueue.hpp>

namespace oc::app {

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

void OpenControlApp::begin() {
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
    uint32_t startTime = 0;
    if constexpr (oc::ENABLE_STATS) {
        startTime = time_provider_();
    }

    uint32_t now = time_provider_();

    // Process tick BEFORE hardware updates to update time state
    if (input_binding_) {
        input_binding_->processTick();
    }

    if (midi_) {
        midi_->update();
    }
    if (frames_) {
        frames_->update();
    }
    if (encoders_) {
        encoders_->update();
    }
    if (buttons_) {
        buttons_->update(now);
    }

    for (auto& hook : pre_context_update_hooks_) {
        if (hook) {
            hook();
        }
    }

    contexts_->update();

    // Flush deferred signal notifications (coalesced updates)
    state::NotificationQueue::instance().flush();

    // Track timing stats
    if constexpr (oc::ENABLE_STATS) {
        uint32_t endTime = time_provider_();
        // Convert from ms to us (assuming millis() provider)
        uint32_t durationUs = (endTime - startTime) * 1000;
        timing_stats_.lastUpdateUs = durationUs;
        timing_stats_.totalUpdateUs += durationUs;
        timing_stats_.totalUpdates++;
        if (durationUs > timing_stats_.peakUpdateUs) {
            timing_stats_.peakUpdateUs = durationUs;
        }
    }
}

void OpenControlApp::dumpStats() const {
    if constexpr (oc::ENABLE_STATS) {
        OC_LOG_INFO("=== Open Control Stats ===");

        // Timing stats
        OC_LOG_INFO("Timing: last={}us, avg={}us, peak={}us ({} updates)",
                    timing_stats_.lastUpdateUs, timing_stats_.avgUpdateUs(),
                    timing_stats_.peakUpdateUs, timing_stats_.totalUpdates);

        // EventBus stats
        const auto& bus = event_bus_.stats();
        OC_LOG_INFO("EventBus: subscribers={} (peak={}), emitted={}, compactions={}",
                    event_bus_.getSubscriberCount(), bus.peakSubscribers,
                    bus.totalEmitted, bus.totalCompactions);
        if (bus.overflowCount > 0) {
            OC_LOG_WARN("  -> {} subscriptions rejected (overflow)", bus.overflowCount);
        }

        // NotificationQueue stats
        const auto& queue = state::NotificationQueue::instance().stats();
        OC_LOG_INFO("NotificationQueue: pending={} (peak={}), coalesced={}, flushed={}",
                    state::NotificationQueue::instance().pendingCount(), queue.peakPending,
                    queue.totalCoalesced, queue.totalFlushed);
        if (state::NotificationQueue::instance().hasOverflowed()) {
            OC_LOG_WARN("  -> {} notifications dropped (overflow)",
                        state::NotificationQueue::instance().overflowCount());
        }

        // Memory estimation
        OC_LOG_INFO("Memory: ~{} bytes estimated", estimateMemoryUsage());
    } else {
        OC_LOG_INFO("Stats disabled. Rebuild with -DOC_ENABLE_STATS=1");
    }
}

void OpenControlApp::resetStats() {
    if constexpr (oc::ENABLE_STATS) {
        event_bus_.resetStats();
        state::NotificationQueue::instance().resetStats();
        state::NotificationQueue::instance().resetOverflowCount();
        timing_stats_ = {};
    }
}

size_t OpenControlApp::estimateMemoryUsage() const {
    size_t total = 0;

    // OpenControlApp base size
    total += sizeof(OpenControlApp);

    // InputBinding state arrays (fixed size from config)
    total += oc::MAX_BUTTONS * (sizeof(bool) * 3 + sizeof(uint32_t) * 2 + sizeof(uint8_t));

    // EventBus subscriptions (estimate based on current usage)
    total += event_bus_.getSubscriberCount() * (sizeof(void*) * 2 + sizeof(bool) + 64);  // 64 for std::function

    // NotificationQueue pending (estimate based on max)
    total += oc::MAX_PENDING_NOTIFICATIONS * (sizeof(void*) + sizeof(size_t) + 64);

    // ContextManager factories and names
    total += oc::MAX_CONTEXTS * (sizeof(void*) * 2);

    return total;
}

}  // namespace oc::app
