# Canonical production source inventory for OpenControl Framework consumers.
# Keep this list sorted and mirror it in library.json's PlatformIO srcFilter.
set(OC_FRAMEWORK_SOURCE_PATHS
    src/oc/api/ButtonAPI.cpp
    src/oc/api/EncoderAPI.cpp
    src/oc/api/MidiAPI.cpp
    src/oc/app/AppBuilder.cpp
    src/oc/app/OpenControlApp.cpp
    src/oc/context/ContextManager.cpp
    src/oc/core/event/EventBus.cpp
    src/oc/core/event/EventTypesMacroSanity.cpp
    src/oc/core/input/BindingHandle.cpp
    src/oc/core/input/ButtonBuilder.cpp
    src/oc/core/input/ComboBuilder.cpp
    src/oc/core/input/EncoderBuilder.cpp
    src/oc/core/input/EncoderLogic.cpp
    src/oc/core/input/GestureDetector.cpp
    src/oc/core/input/InputBinding.cpp
    src/oc/diagnostics/Performance.cpp
    src/oc/log/Log.cpp
    src/oc/state/NotificationQueue.cpp
    src/oc/state/Signal.cpp
    src/oc/time/Time.cpp
)

set(OC_FRAMEWORK_SOURCES ${OC_FRAMEWORK_SOURCE_PATHS})
list(TRANSFORM OC_FRAMEWORK_SOURCES
    PREPEND "${CMAKE_CURRENT_LIST_DIR}/../")
