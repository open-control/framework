// Minimal main for library compilation test
#include <Arduino.h>

// HAL interfaces
#include <oc/hal/Types.hpp>
#include <oc/hal/IDisplayDriver.hpp>
#include <oc/hal/IMidiTransport.hpp>
#include <oc/hal/IEncoderController.hpp>
#include <oc/hal/IButtonController.hpp>
#include <oc/hal/IMultiplexer.hpp>

// Core - Event system
#include <oc/core/event/EventTypes.hpp>
#include <oc/core/event/Event.hpp>
#include <oc/core/event/IEventBus.hpp>
#include <oc/core/event/EventBus.hpp>
#include <oc/core/event/Events.hpp>

// Core - Structs
#include <oc/core/struct/Binding.hpp>

// Core - Input
#include <oc/core/input/InputConfig.hpp>
#include <oc/core/input/InputBinding.hpp>

// Context system
#include <oc/context/IContext.hpp>
#include <oc/context/ContextManager.hpp>

// API
#include <oc/api/ControlAPI.hpp>

void setup() {}
void loop() {}
