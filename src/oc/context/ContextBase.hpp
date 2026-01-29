#pragma once

#include <cassert>
#include <cstddef>
#include <vector>

#include <oc/interface/IContext.hpp>
#include <oc/api/ButtonAPI.hpp>
#include <oc/api/ButtonProxy.hpp>
#include <oc/api/EncoderAPI.hpp>
#include <oc/api/EncoderProxy.hpp>
#include <oc/api/MidiAPI.hpp>
#include <oc/context/APIs.hpp>
#include <oc/context/IContextWithAPIs.hpp>
#include <oc/interface/IContextSwitcher.hpp>
#include <oc/core/input/ButtonBuilder.hpp>
#include <oc/core/input/EncoderBuilder.hpp>
#include <oc/interface/IMidi.hpp>
#include <oc/interface/ITransport.hpp>

#include <oc/core/event/EventTypes.hpp>
#include <oc/core/event/Events.hpp>

namespace oc::context {

/**
 * @brief Base class for application contexts with fluent API
 *
 * Inherits from IContext and provides the fluent API for input bindings
 * (onButton, onEncoder), state proxies, and context switching.
 *
 * Use this class when you need the convenience methods. For simple contexts
 * that don't need the API, inherit directly from IContext.
 *
 * ## Implementation Example
 *
 * @code
 * enum class Button : uint16_t { PLAY = 0 };
 * enum class Encoder : uint16_t { VOLUME = 0 };
 *
 * class MainContext : public ContextBase {
 * public:
 *     static constexpr Requirements REQUIRES{
 *         .button = true,
 *         .encoder = true,
 *         .midi = false
 *     };
 *
 *     oc::type::Result<void> init() override {
 *         onButton(Button::PLAY).press().then([this] { play(); });
 *         onEncoder(Encoder::VOLUME).turn().then([this](float v) { setVolume(v); });
 *         return oc::type::Result<void>::ok();
 *     }
 *
 *     void update() override {}
 *     void cleanup() override {}
 *     const char* getName() const override { return "Main"; }
 * };
 * @endcode
 *
 * @see IContext for the pure interface
 * @see Requirements for declaring API dependencies
 */
class ContextBase : public interface::IContext, public IContextWithAPIs {
public:
    ~ContextBase() override {
        // Fallback safety: if cleanup() wasn't called for some reason.
        clearEventSubscriptions_();
    }

    void setAPIs(const APIs& apis) override { apis_ = &apis; }

    // IContext
    void cleanup() final {
        // Framework responsibility: prevent callbacks from outliving the context.
        clearEventSubscriptions_();
        onCleanup();
    }

protected:
    /**
     * @brief Override point for derived contexts
     *
     * cleanup() is final to guarantee safe teardown ordering.
     */
    virtual void onCleanup() {}

protected:
    // ─────────────────────────────────────────────────────────────────────
    // Event subscriptions (auto-unsubscribed in destructor)
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Event bus proxy that auto-tracks subscriptions
     */
    class EventsProxy {
    public:
        explicit EventsProxy(ContextBase& ctx) : ctx_(ctx) {}

        [[nodiscard]] interface::SubscriptionID on(oc::type::EventCategoryType category,
                                                   oc::type::EventType type,
                                                   interface::EventCallback callback) {
            return ctx_.subscribe_(category, type, std::move(callback));
        }

        void emit(const oc::type::Event& event) {
            ctx_.rawEvents_().emit(event);
        }

        void off(interface::SubscriptionID id) {
            ctx_.unsubscribe_(id);
        }

    private:
        ContextBase& ctx_;
    };

    /**
     * @brief Access a scoped event bus (subscriptions auto-cleaned)
     */
    [[nodiscard]] EventsProxy events() {
        assert(apis_ && "setAPIs() not called");
        return EventsProxy(*this);
    }

    /**
     * @brief Access the raw event bus (advanced)
     *
     * If you subscribe directly, you must unsubscribe manually.
     */
    interface::IEventBus& rawEvents() {
        assert(apis_ && "setAPIs() not called");
        return apis_->events;
    }

    /**
     * @brief Typed MIDI CC subscription (auto-cleaned)
     */
    interface::SubscriptionID onMidiCC(interface::IMidi::CCCallback cb) {
        return subscribe_(core::event::EventCategory::MIDI, core::event::MidiEvent::CC,
                          [cb = std::move(cb)](const oc::type::Event& e) mutable {
                              const auto& evt = static_cast<const core::event::MidiCCEvent&>(e);
                              cb(evt.channel, evt.controller, evt.value);
                          });
    }

    interface::SubscriptionID onMidiNoteOn(interface::IMidi::NoteCallback cb) {
        return subscribe_(core::event::EventCategory::MIDI, core::event::MidiEvent::NOTE_ON,
                          [cb = std::move(cb)](const oc::type::Event& e) mutable {
                              const auto& evt = static_cast<const core::event::MidiNoteOnEvent&>(e);
                              cb(evt.channel, evt.note, evt.velocity);
                          });
    }

    interface::SubscriptionID onMidiNoteOff(interface::IMidi::NoteCallback cb) {
        return subscribe_(core::event::EventCategory::MIDI, core::event::MidiEvent::NOTE_OFF,
                          [cb = std::move(cb)](const oc::type::Event& e) mutable {
                              const auto& evt = static_cast<const core::event::MidiNoteOffEvent&>(e);
                              cb(evt.channel, evt.note, evt.velocity);
                          });
    }

    interface::SubscriptionID onMidiSysEx(interface::IMidi::SysExCallback cb) {
        return subscribe_(core::event::EventCategory::MIDI, core::event::MidiEvent::SYSEX,
                          [cb = std::move(cb)](const oc::type::Event& e) mutable {
                              const auto& evt = static_cast<const core::event::SysExEvent&>(e);
                              cb(evt.data, static_cast<size_t>(evt.length));
                          });
    }
    // ─────────────────────────────────────────────────────────────────────
    // Input Binding Builders (fluent API)
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Start building a button binding
     *
     * @tparam ID Enum class or integral type convertible to oc::type::ButtonID
     * @param id Button identifier
     * @return ButtonBuilder for chaining configuration
     *
     * @code
     * onButton(Button::PLAY).press().then([this] { transport.play(); });
     * @endcode
     */
    template <typename ID>
    [[nodiscard]] core::input::ButtonBuilder onButton(ID id) {
        assert(apis_ && "setAPIs() not called - context not properly initialized");
        assert(apis_->button && "ButtonAPI not available");
        return apis_->button->button(static_cast<oc::type::ButtonID>(id));
    }

    /**
     * @brief Start building an encoder binding
     *
     * @tparam ID Enum class or integral type convertible to oc::type::EncoderID
     * @param id Encoder identifier
     * @return EncoderBuilder for chaining configuration
     *
     * @code
     * onEncoder(Encoder::VOLUME).turn().then([this](float v) { setVolume(v); });
     * @endcode
     */
    template <typename ID>
    [[nodiscard]] core::input::EncoderBuilder onEncoder(ID id) {
        assert(apis_ && "setAPIs() not called - context not properly initialized");
        assert(apis_->encoder && "EncoderAPI not available");
        return apis_->encoder->encoder(static_cast<oc::type::EncoderID>(id));
    }

    // ─────────────────────────────────────────────────────────────────────
    // State Proxies (for querying input state)
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Get a proxy for querying button state
     */
    template <typename ID>
    api::ButtonProxy button(ID id) {
        assert(apis_ && "setAPIs() not called");
        assert(apis_->button && "ButtonAPI not available");
        return api::ButtonProxy(*apis_->button, static_cast<oc::type::ButtonID>(id));
    }

    /**
     * @brief Get a proxy for querying encoder state
     */
    template <typename ID>
    api::EncoderProxy encoder(ID id) {
        assert(apis_ && "setAPIs() not called");
        assert(apis_->encoder && "EncoderAPI not available");
        return api::EncoderProxy(*apis_->encoder, static_cast<oc::type::EncoderID>(id));
    }

    // ─────────────────────────────────────────────────────────────────────
    // Global API Access
    // ─────────────────────────────────────────────────────────────────────

    api::ButtonAPI& buttons() {
        assert(apis_ && apis_->button);
        return *apis_->button;
    }

    api::EncoderAPI& encoders() {
        assert(apis_ && apis_->encoder);
        return *apis_->encoder;
    }

    api::MidiAPI& midi() {
        assert(apis_ && apis_->midi);
        return *apis_->midi;
    }

    interface::ITransport& frames() {
        assert(apis_ && apis_->frames);
        return *apis_->frames;
    }

    // NOTE: events() now returns EventsProxy above

    // ─────────────────────────────────────────────────────────────────────
    // Context Switching (deferred - safe to call from update/handlers)
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Request a switch to another context (deferred)
     */
    template <typename ID>
    void switchTo(ID id) {
        assert(apis_ && apis_->contexts);
        apis_->contexts->switchTo(id);
    }

    /**
     * @brief Request a switch to the default context
     */
    void switchToDefault() {
        assert(apis_ && apis_->contexts);
        apis_->contexts->switchToDefault();
    }

    // ─────────────────────────────────────────────────────────────────────
    // Context Queries
    // ─────────────────────────────────────────────────────────────────────

    template <typename ID>
    bool hasContext(ID id) const {
        assert(apis_ && apis_->contexts);
        return apis_->contexts->hasContext(id);
    }

    const char* contextName(uint8_t id) const {
        assert(apis_ && apis_->contexts);
        return apis_->contexts->contextName(id);
    }

    uint8_t activeContextId() const {
        assert(apis_ && apis_->contexts);
        return apis_->contexts->activeId();
    }

    uint8_t defaultContextId() const {
        assert(apis_ && apis_->contexts);
        return apis_->contexts->defaultId();
    }

    size_t contextCount() const {
        assert(apis_ && apis_->contexts);
        return apis_->contexts->contextCount();
    }

    template <typename Fn>
    void forEachContext(Fn&& fn) const {
        assert(apis_ && apis_->contexts);
        apis_->contexts->forEachContext(std::forward<Fn>(fn));
    }

    // ─────────────────────────────────────────────────────────────────────
    // Availability Checks
    // ─────────────────────────────────────────────────────────────────────

    bool hasButtons() const { return apis_ && apis_->button != nullptr; }
    bool hasEncoders() const { return apis_ && apis_->encoder != nullptr; }
    bool hasMidi() const { return apis_ && apis_->midi != nullptr; }
    bool hasFrames() const { return apis_ && apis_->frames != nullptr; }

private:
    const APIs* apis_ = nullptr;

    interface::IEventBus& rawEvents_() {
        assert(apis_);
        return apis_->events;
    }

    interface::SubscriptionID subscribe_(oc::type::EventCategoryType category,
                                         oc::type::EventType type,
                                         interface::EventCallback callback) {
        assert(apis_);
        interface::SubscriptionID id = apis_->events.on(category, type, std::move(callback));
        if (id != 0) {
            subscriptions_.push_back(id);
        }
        return id;
    }

    void unsubscribe_(interface::SubscriptionID id) {
        if (!apis_ || id == 0) return;
        apis_->events.off(id);
        for (auto& tracked : subscriptions_) {
            if (tracked == id) {
                tracked = 0;
                break;
            }
        }
    }

    void clearEventSubscriptions_() {
        if (!apis_) return;
        for (auto id : subscriptions_) {
            if (id != 0) {
                apis_->events.off(id);
            }
        }
        subscriptions_.clear();
    }

    std::vector<interface::SubscriptionID> subscriptions_{};
};

}  // namespace oc::context
