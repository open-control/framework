# Open Control Framework

**Hardware abstraction framework for building embedded controllers**

[![Version](https://img.shields.io/badge/version-0.2.0-blue)]()
[![License](https://img.shields.io/badge/license-Apache--2.0-green)]()

> **Alpha Software** - API may change.

---

## Goal

Provide a structured, portable foundation for building hardware controllers:
- **Hardware agnostic** - HAL abstracts platform specifics
- **Protocol flexible** - MIDI and Binary (via bridge)
- **Reactive state** - Signal-based state management with RAII subscriptions
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
- USB Serial for high-bandwidth (via [oc-bridge](https://github.com/open-control/bridge))
- ILI9341 display via [ILI9341_T4](https://github.com/vindar/ILI9341_T4) (DMA)
- Rotary encoders via [EncoderTool](https://github.com/luni64/EncoderTool)

---

## Features

- **HAL Interfaces** - Abstract encoders, buttons, display, MIDI, serial transport
- **Multi-Protocol** - MIDI SysEx and Binary (8-bit via bridge)
- **Reactive State** - Signal<T>, SignalVector<T>, SignalWatcher with RAII subscriptions
- **Result<T>** - Typed error handling (no exceptions)
- **Settings<T>** - Persistent configuration with checksum and migration
- **Logging** - Colored output with timestamps and {} formatting
- **LVGL Integration** - Hardware-independent UI helpers ([ui-lvgl](https://github.com/open-control/ui-lvgl))
- **Input Binding** - Fluent API with gestures (long press, double tap, combos)
- **Encoder Modes** - Normalized, relative, raw with bounds and quantization
- **Context System** - Application modes with clean lifecycle management
- **Event Bus** - Typed, decoupled component communication
- **COBS Codec** - Framing for serial communication

---

## Architecture

See `ARCHITECTURE.md` for the enforced rules (namespaces, module dependencies).

```
oc::
├── type/         # Level 0: Foundational types (no internal dependencies)
│   └── Ids.hpp, Result.hpp, Callbacks.hpp, Event.hpp
├── interface/    # Level 1: Hardware abstraction interfaces
│   ├── IButton, IEncoder, IMidi, IStorage
│   ├── IDisplay, IGpio, IMultiplexer, ITransport
│   └── IContext, IContextSwitcher, IEventBus
├── impl/         # Level 2: Null/memory implementations for testing
│   └── NullMidi, NullTransport, MemoryStorage
├── core/         # Level 2: Core logic
│   ├── event/    # EventBus, typed events
│   └── input/    # InputBinding, EncoderLogic, Builders
├── state/        # Level 2: Reactive state management
│   ├── Signal, SignalVector, SignalString
│   └── SignalWatcher, Settings, NotificationQueue
├── context/      # Level 3: Context management
│   └── ContextManager, ContextBase, APIs
├── api/          # Level 3: User-facing APIs
│   └── ButtonAPI, EncoderAPI, MidiAPI
├── app/          # Level 4: Application entry point
│   └── OpenControlApp, AppBuilder
├── log/          # Logging infrastructure
├── time/         # Time providers
├── codec/        # Protocol codecs (COBS)
├── debug/        # Debug utilities
├── util/         # General utilities
└── Config.hpp    # Configurable limits
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

## Reactive State (Signal)

Signal<T> is the core reactive primitive for state management.

### Basic Usage

```cpp
#include <oc/state/Signal.hpp>

Signal<int> counter{0};

// Subscribe (RAII auto-unsubscribe)
auto sub = counter.subscribe([](const int& val) {
    updateDisplay(val);
});

counter.set(42);  // Triggers callback
// sub goes out of scope → auto-unsubscribe
```

### SignalVector

Fixed-capacity reactive collection (zero heap allocation after construction):

```cpp
#include <oc/state/SignalVector.hpp>

SignalVector<std::string, 32> deviceNames;

auto sub = deviceNames.subscribe([]() {
    rebuildList();  // Called on any structural change
});

// Update from handler
deviceNames.set(names.data(), names.size());
```

### SignalWatcher

Coalesce multiple signal changes into one callback:

```cpp
#include <oc/state/SignalWatcher.hpp>

SignalWatcher watcher;

// Watch multiple signals → one callback
watcher.watchAll(
    [this]() { updateUI(); },
    state.name,
    state.type,
    state.enabled
);

// Or with groups for arrays
auto& group = watcher.group([this]() { updateList(); });
for (auto& signal : state.items) {
    group.watch(signal);
}
```

---

## Result Error Handling

Result<T> provides typed error handling without exceptions:

```cpp
#include <oc/type/Result.hpp>

oc::type::Result<void> initHardware() {
    if (!device.detect()) {
        return oc::type::Result<void>::err(oc::type::ErrorCode::HARDWARE_NOT_FOUND);
    }
    return oc::type::Result<void>::ok();
}

// Usage
auto result = initHardware();
if (result.isErr()) {
    OC_LOG_ERROR("Init failed: {}", oc::type::errorCodeToString(result.error().code));
}
```

### Error Codes

```cpp
oc::type::ErrorCode::HARDWARE_NOT_FOUND    // Device not detected
oc::type::ErrorCode::HARDWARE_INIT_FAILED  // Initialization failed
oc::type::ErrorCode::RESOURCE_EXHAUSTED    // No more capacity
oc::type::ErrorCode::INVALID_ARGUMENT      // Parameter out of range
oc::type::ErrorCode::STORAGE_CORRUPT       // Data integrity check failed
// ... and more
```

---

## Settings Persistence

Settings<T> provides persistent configuration with checksum validation:

```cpp
#include <oc/state/Settings.hpp>

struct MySettings {
    uint8_t midiChannel = 1;
    float volume = 0.5f;
    char presetName[32] = "Default";
};

EEPROMBackend eeprom;
Settings<MySettings> settings(eeprom, 0x0000, /*version=*/1);

// Load (returns defaults on corruption/version mismatch)
settings.load();

// Read
settings.get().midiChannel;

// Modify (dirty tracking)
settings.modify([](auto& s) { s.volume = 0.75f; });

// Save (only if dirty)
settings.save();
```

---

## Logging

Lightweight logging with colored output and timestamps:

```cpp
// In setup():
#include <oc/teensy/TeensyOutput.hpp>
oc::log::setOutput(oc::teensy::logOutput());

// Anywhere:
OC_LOG_DEBUG("Value: {}", x);   // [12ms] DEBUG: Value: 42  (cyan)
OC_LOG_INFO("Boot OK");         // [15ms] INFO: Boot OK     (green)
OC_LOG_WARN("Low: {}%", pct);   // [20ms] WARN: Low: 5%     (yellow)
OC_LOG_ERROR("Fail: {}", msg);  // [25ms] ERROR: Fail: ...  (red)
```

Enable/disable via build flag: `-DOC_LOG`

---

## Serial Transport & COBS

For high-bandwidth communication via [oc-bridge](https://github.com/open-control/bridge):

### ISerialTransport

```cpp
#include <oc/interface/ITransport.hpp>

class MySerialTransport : public oc::interface::ITransport {
    void send(const uint8_t* data, size_t length) override {
        // COBS-encode and transmit
    }

    void setOnReceive(ReceiveCallback cb) override {
        onReceive_ = std::move(cb);
    }
};
```

### COBS Codec

```cpp
#include <oc/codec/CobsCodec.hpp>

// Encode
uint8_t encoded[oc::codec::cobsMaxEncodedSize(dataLen)];
size_t encodedLen = oc::codec::cobsEncode(data, dataLen, encoded);

// Streaming decode
oc::codec::CobsDecoder<4096> decoder;
decoder.feed(byte, [](const uint8_t* data, size_t len) {
    processFrame(data, len);
});
```

---

## Contexts

A context represents an application mode. Only one is active at a time.

```cpp
class MyContext : public oc::context::ContextBase {
public:
    static constexpr oc::context::Requirements REQUIRES{
        .button = true,
        .encoder = true,
        .midi = true
    };

    const char* getName() const override { return "MyContext"; }

    oc::type::Result<void> init() override {
        // Setup bindings here
        return oc::type::Result<void>::ok();
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
button(BTN_1).clearLatch();

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

## Configuration

Override compile-time limits via PlatformIO build flags:

```ini
; platformio.ini
build_flags =
    -DOC_MAX_BUTTONS=32
    -DOC_MAX_ENCODERS=8
    -DOC_MAX_BUTTON_BINDINGS=64
    -DOC_MAX_ENCODER_BINDINGS=32
    -DOC_ENABLE_STATS=1
    -DOC_LOG              ; Enable logging
```

See [Configuration](https://github.com/open-control/framework/wiki/Configuration) for all options.

---

## Testing

```bash
cd framework
pio test -e native
```

200+ unit tests covering core components.

---

## Related Projects

- [hal-teensy](https://github.com/open-control/hal-teensy) - Teensy 4.x HAL implementation
- [ui-lvgl](https://github.com/open-control/ui-lvgl) - LVGL UI integration
- [ui-lvgl-components](https://github.com/open-control/ui-lvgl-components) - Custom LVGL widgets
- [protocol-codegen](https://github.com/open-control/protocol-codegen) - Protocol code generator
- [bridge](https://github.com/open-control/bridge) - Serial-to-UDP bridge (Rust)

---

## Contributing

Contributions welcome. See [issues](https://github.com/open-control/framework/issues).

---

## License

[Apache License 2.0](LICENSE)

---

**[petitechose.audio](https://petitechose.audio)**
