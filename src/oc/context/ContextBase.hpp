#pragma once

#include <cassert>

#include <oc/interface/IContext.hpp>
#include <oc/api/ButtonAPI.hpp>
#include <oc/api/ButtonProxy.hpp>
#include <oc/api/EncoderAPI.hpp>
#include <oc/api/EncoderProxy.hpp>
#include <oc/api/MidiAPI.hpp>
#include <oc/context/APIs.hpp>
#include <oc/interface/IContextSwitcher.hpp>
#include <oc/core/input/ButtonBuilder.hpp>
#include <oc/core/input/EncoderBuilder.hpp>
#include <oc/interface/ITransport.hpp>

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
 * class MainContext : public ContextBase {
 * public:
 *     static constexpr Requirements REQUIRES{
 *         .button = true,
 *         .encoder = true,
 *         .midi = false
 *     };
 *
 *     oc::type::Result<void> init() override {
 *         onButton(oc::type::ButtonID::PLAY).press().then([this] { play(); });
 *         onEncoder(oc::type::EncoderID::VOLUME).turn().then([this](float v) { setVolume(v); });
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
class ContextBase : public interface::IContext {
public:
    ~ContextBase() override = default;

    /**
     * @brief Inject API references (called by ContextManager before initialize)
     * @param apis Reference to the APIs container
     * @note Do not call this directly - managed by ContextManager
     */
    void setAPIs(const APIs& apis) override { apis_ = &apis; }

protected:
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
     * onButton(oc::type::ButtonID::PLAY).press().then([this] { transport.play(); });
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
     * onEncoder(oc::type::EncoderID::VOLUME).turn().then([this](float v) { setVolume(v); });
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

    interface::IEventBus& events() {
        assert(apis_);
        return apis_->events;
    }

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
};

}  // namespace oc::context
