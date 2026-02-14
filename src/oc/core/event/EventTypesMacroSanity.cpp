// Compile-time sanity check for Windows macro collisions.
//
// Some Windows headers define ERROR as a macro. If any public header uses
// the identifier `ERROR` (or similar macro-prone names) it can break builds
// depending on include order.
//
// This TU forces ERROR to be defined while including EventTypes.hpp so any
// regression is caught at compile time.

#if !defined(ERROR)
#define ERROR 0
#define OC__EVENTTYPES__DEFINED_ERROR_MACRO 1
#endif

#include <oc/core/event/EventTypes.hpp>

namespace oc::core::event {

static_assert(SystemEvent::ERROR_EVENT == 4002,
              "Unexpected SystemEvent::ERROR_EVENT value");

}  // namespace oc::core::event

#if defined(OC__EVENTTYPES__DEFINED_ERROR_MACRO)
#undef ERROR
#undef OC__EVENTTYPES__DEFINED_ERROR_MACRO
#endif
