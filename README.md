# Open Control Framework

**Hardware abstraction framework for building embedded controllers**

[![Version](https://img.shields.io/badge/version-0.1.0--alpha-blue)]()
[![License](https://img.shields.io/badge/license-Apache--2.0-green)]()

> **Alpha Software** - API may change.

---

## Goal

Provide a structured, portable foundation for building hardware controllers:
- **Hardware agnostic** - HAL abstracts platform specifics
- **Protocol flexible** - MIDI supported, OSC planned
- **Scalable** - From simple 4-encoder boxes to complex instruments

---

## Platform Support

| Platform | Status |
|----------|--------|
| Teensy 4.x (ARM Cortex-M7) | Supported |
| STM32 (Daisy-like) | Planned |
| ESP32 | Planned |

### Teensy 4.x

See [hal-teensy](https://github.com/open-control/hal-teensy):
- USB MIDI native
- ILI9341 display via [ILI9341_T4](https://github.com/vindar/ILI9341_T4) (DMA)
- Rotary encoders via [EncoderTool](https://github.com/luni64/EncoderTool)

---

## Features

- **HAL Interfaces** - Abstract encoders, buttons, display, transport
- **Multi-Protocol** - MIDI supported, OSC planned
- **LVGL Integration** - Hardware-independent UI helpers ([ui-lvgl](https://github.com/open-control/ui-lvgl))
- **Input Binding** - Fluent API with gestures (long press, double tap, combos)
- **Encoder Modes** - Normalized, relative, raw with bounds and quantization
- **Context System** - Application modes with clean lifecycle management
- **Event Bus** - Typed, decoupled component communication

---

## Architecture

```
oc::
├── hal/          # Hardware abstraction interfaces
├── core/
│   ├── event/    # EventBus, typed events
│   └── input/    # InputBinding, EncoderLogic, Builders
├── context/      # IContext, ContextManager
├── api/          # ButtonAPI, EncoderAPI, MidiAPI
└── app/          # OpenControlApp, AppBuilder
```

---

## Quick Start

```cpp
#include <oc/app/AppBuilder.hpp>
#include <oc/app/OpenControlApp.hpp>

std::optional<oc::app::OpenControlApp> app;

void setup() {
    app = oc::app::AppBuilder()
        .timeProvider(millis)
        .buttons(std::make_unique<MyButtonController>())
        .encoders(std::make_unique<MyEncoderController>())
        .midi(std::make_unique<MyMidiDriver>())
        .inputConfig({.longPressMs = 500, .doubleTapWindowMs = 300})
        .build();

    app->registerContext<MyContext>(ContextID::MAIN, "Main");
    app->begin();
}

void loop() {
    app->update();
}
```

> **Teensy users**: See [hal-teensy](https://github.com/open-control/hal-teensy) for a simplified `oc::teensy::AppBuilder` that auto-configures drivers.

---

## Contexts

A context represents an application mode. Only one is active at a time.

```cpp
class MyContext : public oc::context::IContext {
public:
    // Declare required APIs (validated at registration)
    static constexpr oc::context::Requirements REQUIRES{
        .button = true,
        .encoder = true,
        .midi = true
    };

    const char* getName() const override { return "MyContext"; }

    bool initialize() override {
        // Setup bindings here
        return true;
    }

    void update() override {
        // Called every loop
    }

    void cleanup() override {
        // Called before switching away
    }
};
```

### Lifecycle

```
registerContext() → begin() → initialize() → update()* → cleanup() → [switch]
```

### Switching

```cpp
app.contexts().switchTo(contextId);
app.contexts().switchToDefault();
```

---

## Button Bindings

### From IContext

```cpp
// Binding syntax: onButton(id).<gesture>().<modifiers>().then(callback)

onButton(BTN_1).press().then([]{ /* pressed */ });
onButton(BTN_1).release().then([]{ /* released */ });
onButton(BTN_1).longPress(800).then([]{ /* held 800ms */ });
onButton(BTN_1).doubleTap().then([]{ /* double tap */ });
onButton(BTN_1).combo(BTN_2).then([]{ /* both pressed */ });

// With modifiers
onButton(BTN_1).press().latch().then([]{ /* toggle behavior */ });
onButton(BTN_1).press().scope(MENU_SCOPE).then([]{ /* scoped */ });
```

### State Access

```cpp
// Via proxy (single button)
button(BTN_1).isPressed();
button(BTN_1).isLatched();
button(BTN_1).setLatch(true);

// Predicate for conditional bindings
onEncoder(ENC_1).turn()
    .when(button(BTN_SHIFT).pressed())
    .then([](float v){ /* only while BTN_SHIFT held */ });

// Global operations
buttons().clearBindings();
buttons().clearScope(MENU_SCOPE);
```

---

## Encoder Bindings

### From IContext

```cpp
// Basic turn binding
onEncoder(ENC_1).turn().then([](float value) {
    // value depends on mode
});

// Conditional binding
onEncoder(ENC_1).turn()
    .when(button(BTN_SHIFT).pressed())
    .then([](float v){ fineAdjust(v); });
```

### Modes

| Mode | Output | Use Case |
|------|--------|----------|
| `NORMALIZED` | `0.0` - `1.0` (or custom bounds) | Volume, parameters |
| `RELATIVE` | `+delta` / `-delta` per detent | Scrolling |
| `RAW` | Accumulated tick count | Position tracking |

### Configuration via Proxy

```cpp
// Mode and bounds
encoder(ENC_1).setMode(EncoderMode::NORMALIZED);
encoder(ENC_1).setBounds(0.0f, 127.0f);
encoder(ENC_1).setDiscreteSteps(128);  // Quantize

// Relative mode
encoder(ENC_2).setMode(EncoderMode::RELATIVE);
encoder(ENC_2).setDelta(1.0f);  // +1/-1 per detent

// External sync (e.g., DAW feedback)
encoder(ENC_1).setPosition(0.5f);

// Read current position
float pos = encoder(ENC_1).position();
```

### Global Operations

```cpp
encoders().clearBindings();
encoders().clearScope(MENU_SCOPE);
encoders().setPosition(ENC_1, 0.5f);
```

---

## MIDI

```cpp
midi().sendNoteOn(channel, note, velocity);
midi().sendNoteOff(channel, note);
midi().sendCC(channel, cc, value);
```

---

## Event Bus

Low-level pub/sub for decoupled communication.

```cpp
// Subscribe (category + type)
auto id = events().on(
    EventCategory::INPUT,
    EventType::BUTTON_PRESSED,
    [](const Event& e) {
        auto& evt = static_cast<const ButtonPressedEvent&>(e);
        // handle evt.buttonId
    });

// Emit
events().emit(ButtonPressedEvent{buttonId});

// Unsubscribe
events().off(id);
```

> Note: For button/encoder handling, prefer the fluent binding API (`onButton`, `onEncoder`) over raw events.

---

## Scopes

Group bindings for bulk cleanup:

```cpp
constexpr ScopeID MENU_SCOPE = 1;

onButton(BTN_1).press().scope(MENU_SCOPE).then([]{ /* ... */ });
onButton(BTN_2).press().scope(MENU_SCOPE).then([]{ /* ... */ });

// Clear all at once
buttons().clearScope(MENU_SCOPE);
```

---

## Testing

```bash
cd framework
pio test -e native
```

74 unit tests covering core components.

---

## Contributing

Contributions welcome. See [issues](https://github.com/open-control/framework/issues).

---

## License

[Apache License 2.0](LICENSE)

---

**[petitechose.audio](https://petitechose.audio)**
