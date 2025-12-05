/**
 * @file main.cpp
 * @brief COMPILATION TEST ONLY - Not a functional example
 *
 * This file exists solely to verify that the framework compiles correctly
 * during development. It includes all public headers to catch compilation errors.
 *
 * FOR USAGE EXAMPLES, see: examples/minimal-teensy41/
 *
 * As a library consumer, you should NOT include this file.
 * Create your own main.cpp using AppBuilder to compose your application.
 */

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

// App
#include <oc/app/AppBuilder.hpp>
#include <oc/app/OpenControlApp.hpp>

void setup() {}
void loop() {}
