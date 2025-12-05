# Teensy 4.1 Minimal Example

A minimal Open Control Framework example demonstrating:
- 4 rotary encoders → MIDI CC output
- 2 buttons with press, release, and long press handling
- Clean architecture with `AppBuilder` and `ControlAPI`

## Hardware Requirements

- Teensy 4.1
- 4x Rotary encoders (quadrature, 24 PPR recommended)
- 2x Momentary push buttons
- USB cable for MIDI and power

## Default Wiring

| Component | Pin A | Pin B | Notes |
|-----------|-------|-------|-------|
| Encoder 1 | 2 | 3 | Main volume |
| Encoder 2 | 4 | 5 | Pan |
| Encoder 3 | 6 | 7 | Send A |
| Encoder 4 | 8 | 9 | Send B |
| Button 1 | 10 | GND | Play/Stop |
| Button 2 | 11 | GND | Record |

> Buttons use internal pull-up resistors. Connect one leg to the pin, the other to GND.

## MIDI Mapping

| Control | MIDI Message | Channel |
|---------|--------------|---------|
| Encoder 1 | CC 16 (0-127) | 1 |
| Encoder 2 | CC 17 (0-127) | 1 |
| Encoder 3 | CC 18 (0-127) | 1 |
| Encoder 4 | CC 19 (0-127) | 1 |
| Button 1 Press | CC 20 = 127 | 1 |
| Button 1 Release | CC 20 = 0 | 1 |
| Button 2 Toggle | CC 21 = 127/0 | 1 |

## Quick Start

### 1. Install PlatformIO

```bash
# VS Code extension (recommended)
# Or CLI: pip install platformio
```

### 2. Clone and Build

```bash
cd examples/teensy41_minimal
pio run
```

### 3. Upload

```bash
pio run -t upload
```

### 4. Monitor Serial Output

```bash
pio device monitor -b 115200
```

## Project Structure

```
teensy41_minimal/
├── include/
│   └── Config.hpp      # Hardware pin definitions (edit this!)
├── src/
│   └── main.cpp        # Application entry point
├── platformio.ini      # Build configuration
└── README.md           # This file
```

## Customization

### Change Pin Assignments

Edit `include/Config.hpp`:

```cpp
constexpr std::array<oc::drivers::common::EncoderDef, 4> ENCODERS = {{
    {.id = 1, .pinA = 2, .pinB = 3, ...},  // Change pins here
    // ...
}};
```

### Change MIDI Mapping

Edit `include/Config.hpp`:

```cpp
constexpr uint8_t MIDI_CHANNEL = 0;      // 0-15 (displayed as 1-16)
constexpr uint8_t ENCODER_CC_BASE = 16;  // First encoder CC number
constexpr uint8_t BUTTON1_CC = 20;
constexpr uint8_t BUTTON2_CC = 21;
```

### Add More Inputs

1. Update array sizes in `Config.hpp`
2. Add definitions to the arrays
3. Add bindings in `MinimalContext::setupEncoderBindings()` or `setupButtonBindings()`

### Change Encoder Behavior

```cpp
// In Config.hpp, per encoder:
.rangeAngle = 270,      // Degrees for full 0-1 range (270° = ~3/4 turn)
.ticksPerEvent = 4,     // Events per detent (4 = smooth, 1 = sensitive)
.invertDirection = true // Flip direction if wired backwards
```

## Code Walkthrough

### 1. Configuration (`Config.hpp`)

All hardware is defined as `constexpr` for zero runtime overhead:

```cpp
constexpr std::array<EncoderDef, 4> ENCODERS = {{...}};
constexpr std::array<ButtonDef, 2> BUTTONS = {{...}};
```

### 2. Context (`MinimalContext`)

Implements `IContext` interface with input bindings:

```cpp
class MinimalContext : public oc::context::IContext {
    bool initialize(oc::api::ControlAPI& api) override {
        // Set up bindings here
        api.onTurned(encoderId, [](float value) { ... });
        api.onPressed(buttonId, []() { ... });
    }
};
```

### 3. Application Setup (`main.cpp`)

Uses `AppBuilder` for clean dependency injection:

```cpp
app = oc::app::AppBuilder()
    .timeProvider(millis)
    .midi(std::make_unique<TeensyUsbMidi>())
    .encoders(std::make_unique<EncoderController<4>>(ENCODERS))
    .buttons(std::make_unique<ButtonController<2>>(BUTTONS))
    .build();

app.registerContext<MinimalContext>("minimal");
app.begin();
```

### 4. Main Loop

Single call updates everything:

```cpp
void loop() {
    app.update();  // Polls inputs, processes events, updates context
}
```

## Input Binding API Reference

### Button Bindings

```cpp
// Simple press
api.onPressed(buttonId, []() { /* action */ });

// Release
api.onReleased(buttonId, []() { /* action */ });

// Long press (custom duration)
api.onLongPress(buttonId, []() { /* action */ }, 500);  // 500ms

// Double tap
api.onDoubleTap(buttonId, []() { /* action */ }, 300);  // 300ms window
```

### Encoder Bindings

```cpp
// Turn (value is 0.0-1.0 in NORMALIZED mode)
api.onTurned(encoderId, [](float value) {
    uint8_t midi = value * 127;  // Map to MIDI range
});

// Turn while button held
api.onTurnedWhilePressed(encoderId, buttonId, [](float value) { ... });
```

### MIDI Output

```cpp
api.sendCC(channel, cc, value);           // Control Change
api.sendNoteOn(channel, note, velocity);  // Note On
api.sendNoteOff(channel, note, velocity); // Note Off
```

## Troubleshooting

### No MIDI Output

1. Check USB mode is `USB_MIDI_SERIAL` in `platformio.ini`
2. Verify Teensy appears as MIDI device in your DAW
3. Check serial monitor for debug messages

### Encoder Direction Wrong

Set `invertDirection = true` in the encoder definition.

### Encoder Too Sensitive/Not Sensitive

Adjust `ticksPerEvent`:
- `1` = Very sensitive (event per tick)
- `4` = Standard (event per detent)
- `8` = Less sensitive

### Button Not Responding

1. Check wiring (button should connect pin to GND)
2. Verify `activeLow = true` for pull-up configuration
3. Increase `DEBOUNCE_MS` if bouncing

## License

Apache 2.0 - See [LICENSE](../../LICENSE)
