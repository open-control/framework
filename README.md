# Open Control Framework

**Hardware abstraction framework for MIDI controllers with optional LVGL UI**

[![Version](https://img.shields.io/badge/version-0.1.0-blue)]()
[![License](https://img.shields.io/badge/license-Apache--2.0-green)]()
[![Platform](https://img.shields.io/badge/platform-Teensy%204.x-orange)]()

---

## Overview

Open Control is a PlatformIO library providing:

- **HAL Interfaces** - Abstract hardware (display, encoders, buttons, MIDI)
- **Event Bus** - Decoupled component communication
- **Context System** - Extensible architecture for different use cases
- **Input Binding** - Gesture recognition (long press, double tap, combos)
- **Optional LVGL** - UI components when needed

---

## Architecture

```
oc::
├── hal/          # Hardware Abstraction Layer interfaces
├── core/         # EventBus, InputBinding, types
├── context/      # IContext, ContextManager
├── api/          # ControlAPI (facade for contexts)
├── app/          # OpenControlApp, AppBuilder
└── ui/           # IView, widgets (optional LVGL)

drivers/
└── teensy/       # Teensy 4.x implementations
```

---

## Quick Start

### As Library Dependency

```ini
# platformio.ini
[env:teensy41]
platform = teensy
board = teensy41
framework = arduino

lib_deps =
    https://github.com/open-control/framework.git

build_flags =
    -std=gnu++17
    -D USB_MIDI_SERIAL
```

```cpp
// main.cpp
#include <oc/app/AppBuilder.hpp>

oc::app::OpenControlApp app;

void setup() {
    app = oc::app::AppBuilder()
        .midi(std::make_unique<TeensyUsbMidi>())
        .encoders(std::make_unique<TeensyEncoderController<4>>(config))
        .buttons(std::make_unique<TeensyButtonController<8>>(config))
        .build();

    app.registerContext<MyContext>("main");
    app.contexts().switchTo("main");
    app.begin();
}

void loop() {
    app.update();
}
```

---

## Supported Hardware

| Component | Driver |
|-----------|--------|
| Teensy 4.0/4.1 | Native USB MIDI |
| ILI9341 Display | SPI with DMA |
| Rotary Encoders | Via EncoderTool |
| Buttons | Direct GPIO or CD74HC4067 mux |

---

## Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| [ILI9341_T4](https://github.com/vindar/ILI9341_T4) | ^1.6.0 | Optimized Teensy display |
| [LVGL](https://lvgl.io/) | ^9.x | UI framework (optional) |
| [EncoderTool](https://github.com/luni64/EncoderTool) | latest | Encoder handling |

---

## License

[Apache License 2.0](LICENSE)

---

**Built by [petitechose.audio](https://petitechose.audio)**
