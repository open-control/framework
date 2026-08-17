#include "Signal.hpp"

#include <config/PlatformCompat.hpp>
#include <oc/log/Log.hpp>

namespace oc::state::detail {

FLASHMEM const SubscriptionDebugContext*& currentSubscriptionDebugContext() {
    static const SubscriptionDebugContext* context = nullptr;
    return context;
}

FLASHMEM void reportSignalSubscriberOverflow(const char* signalLabel,
                                             size_t subscriberCount,
                                             size_t maxSubscribers,
                                             const void* signalAddress) {
    [[maybe_unused]] const auto* context = currentSubscriptionDebugContext();
    OC_LOG_ERROR(
        "[Signal] MaxSubscribers exceeded label={} subscribers={} max={} requester={} address={}",
        signalLabel ? signalLabel : "<unnamed>",
        subscriberCount,
        maxSubscribers,
        (context && context->requesterLabel) ? context->requesterLabel : "<unknown>",
        signalAddress
    );
}

}  // namespace oc::state::detail
