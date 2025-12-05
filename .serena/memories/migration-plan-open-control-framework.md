# Plan de Migration Open Control Framework

## Progression

| Phase | Statut | Date | Notes |
|-------|--------|------|-------|
| **0** | ✅ | 2025-12-04 | Nettoyage repo, Apache 2.0 |
| **1** | ✅ | 2025-12-04 | Structure + HAL Interfaces |
| **2** | ✅ | 2025-12-04 | Core Logic (EventBus, InputBinding) |
| **3** | ✅ | 2025-12-04 | Context System |
| **4** | ✅ | 2025-12-04 | ControlAPI |
| **5** | ✅ | 2025-12-04 | OpenControlApp + AppBuilder |
| **5.1** | ✅ | 2025-12-05 | Audit naming + IsActiveFn refactor |
| **5.2** | ✅ | 2025-12-05 | doubleTapWindowMs, EncoderMode 3 valeurs |
| **6.1** | ✅ | 2025-12-05 | Modifications HAL (prérequis Phase 6) |
| 6.2 | 🔴 | - | Câblage HAL → EventBus |
| 6.3 | 🔴 | - | Drivers Arduino |
| 6.4 | 🔴 | - | Drivers Teensy |
| 7 | 🔴 | - | UI Optionnel |
| 8 | 🔴 | - | Example Minimal |
| 9 | ⏸️ | - | Adapter midi-studio/core |
| 10 | ⏸️ | - | Adapter context-bitwig |

---

# PHASES ACCOMPLIES (Synthèse)

## Phase 0 : Nettoyage ✅

- Supprimé : `asset/font/`, `docs/`, `script/midi/`, scripts PIO spécifiques
- Mis à jour : `library.json` (open-control 0.1.0), `LICENSE` (Apache 2.0)
- Conservé : `.clang-format`, `.clangd`, `.vscode/`, scripts LVGL génériques

## Phases 1-5 : Framework Core ✅

**Fichiers créés :**

| Catégorie | Fichiers |
|-----------|----------|
| **HAL** | `Types.hpp`, `IDisplayDriver.hpp`, `IMidiTransport.hpp`, `IEncoderController.hpp`, `IButtonController.hpp`, `IMultiplexer.hpp` |
| **Core** | `Event.hpp`, `IEventBus.hpp`, `EventBus.hpp/.cpp`, `Events.hpp`, `EventTypes.hpp`, `Binding.hpp`, `InputBinding.hpp/.cpp`, `InputConfig.hpp` |
| **Context** | `IContext.hpp`, `ContextManager.hpp/.cpp` |
| **API** | `ControlAPI.hpp/.cpp` |
| **App** | `OpenControlApp.hpp/.cpp`, `AppBuilder.hpp/.cpp` |

**Décisions architecturales clés :**
- EventBus séparé .hpp/.cpp (pas header-only)
- `IsActiveFn` au lieu de `lv_obj_t*` (découplage LVGL)
- `TimeProvider` injecté (portabilité bare-metal/FreeRTOS)
- `assert()` au lieu de `throw` (embedded)
- Namespaces : `oc::hal`, `oc::core`, `oc::context`, `oc::api`, `oc::app`

## Phase 5.1-5.2 : Refinements ✅

- Ordre params scoped : `(scope, latch = false, isActive = nullptr)`
- `doubleTapWindowMs` per-binding (comme `longPressMs`)
- EncoderMode : `NORMALIZED` | `RAW` | `RELATIVE`
- Encoder API en `float` [0.0-1.0]

## Phase 6.1 : Modifications HAL ✅

| Interface | Modifications |
|-----------|---------------|
| **Toutes** | `init()` → retourne `bool` |
| `Types.hpp` | Supprimé `activeLow` de `GpioPin` (→ driver's `ButtonDef`) |
| `IMultiplexer` | Renommé `readDigital()`, ajouté `readAnalog()`, `supportsAnalog()` |
| `IMidiTransport` | Ajouté `sendChannelPressure()`, `allNotesOff()` |
| `OpenControlApp` | `begin()` → retourne `bool`, gère échecs init |

---

# PHASES À VENIR (Détail complet)

## Principes Architecturaux

### Framework = Lib unopinionated
- OpenControl fournit des **outils**, pas une app complète
- Supporte plusieurs plateformes (Teensy, ESP32, STM32...)
- Consumer décide comment assembler les outils

### Flux EventBus (Design B - ACTÉ)

```
HAL Drivers (callbacks)
        ↓
OpenControlApp::begin() (wire callbacks → EventBus)
        ↓
EventBus (pub/sub interne)
        ↓
InputBinding (subscribes, détecte gestures)
        ↓
User callbacks (via api.onPressed(), api.onTurned()...)
```

**User ne touche jamais EventBus** - il utilise `api.onPressed()`, le framework gère le reste.

### Responsabilités

| Composant | Responsabilité |
|-----------|----------------|
| HAL interfaces | Contrats abstraits + callbacks |
| EventBus | Pub/sub découplé (interne) |
| InputBinding | Détection gestures, déclenche callbacks user |
| ControlAPI | Façade unifiée pour les Contexts |
| ContextManager | Lifecycle des contextes |
| Drivers | Implémentations HAL spécifiques plateforme |
| **OpenControlApp** | Orchestration + câblage HAL→EventBus |

---

## Phase 6.2 : Câblage HAL → EventBus

### Objectif
Wirer les callbacks HAL vers EventBus dans `OpenControlApp::begin()`.

### Implémentation

**Modifier `OpenControlApp.cpp`** - ajouter dans `begin()` après les `init()` :

```cpp
bool OpenControlApp::begin() {
    // ... existing init code ...

    // ═══════════════════════════════════════════════════
    // Wire HAL callbacks to EventBus
    // ═══════════════════════════════════════════════════
    
    if (buttons_) {
        buttons_->setCallback([this](hal::ButtonID id, hal::ButtonEvent evt) {
            if (evt == hal::ButtonEvent::Pressed) {
                event_bus_.emit(core::event::ButtonPressEvent(id, true));
            } else {
                event_bus_.emit(core::event::ButtonReleaseEvent(id));
            }
        });
    }
    
    if (encoders_) {
        encoders_->setCallback([this](hal::EncoderID id, float value) {
            event_bus_.emit(core::event::EncoderChangedEvent(id, value));
        });
    }

    contexts_->switchToDefault();
    return true;
}
```

### Validation
- [ ] Compiler
- [ ] InputBinding reçoit bien les events quand un driver déclenche le callback

---

## Phase 6.3 : Drivers Arduino (génériques)

### Structure

```
src/drivers/arduino/
├── mux/
│   └── GenericMux.hpp         # Template + aliases
└── input/
    ├── ButtonController.hpp   # Template, gère MCU + MUX
    └── EncoderController.hpp  # Lib Encoder (PJRC)
```

**Namespace** : `oc::drivers::arduino`

### 6.3.1 - GenericMux.hpp

```cpp
#pragma once

#include <Arduino.h>
#include <array>
#include <oc/hal/IMultiplexer.hpp>

namespace oc::drivers::arduino {

template<uint8_t NumPins>
class GenericMux : public hal::IMultiplexer {
    static_assert(NumPins >= 1 && NumPins <= 4, "Mux supports 1-4 select pins");

public:
    struct Config {
        std::array<uint8_t, NumPins> selectPins;
        uint8_t signalPin;
        uint16_t settleTimeUs = 20;
        bool signalPullup = true;
    };

    explicit GenericMux(const Config& cfg) : config_(cfg) {}

    bool init() override {
        for (uint8_t pin : config_.selectPins) {
            pinMode(pin, OUTPUT);
            digitalWrite(pin, LOW);
        }
        pinMode(config_.signalPin, config_.signalPullup ? INPUT_PULLUP : INPUT);
        current_channel_ = 0;
        initialized_ = true;
        return true;
    }

    uint8_t channelCount() const override { return 1 << NumPins; }

    void select(uint8_t channel) override {
        if (!initialized_ || channel >= channelCount()) return;
        if (channel == current_channel_) return;
        
        for (uint8_t i = 0; i < NumPins; ++i) {
            digitalWrite(config_.selectPins[i], (channel >> i) & 0x01);
        }
        current_channel_ = channel;
        delayMicroseconds(config_.settleTimeUs);
    }

    bool readDigital(uint8_t channel) override {
        select(channel);
        return digitalRead(config_.signalPin);
    }

    uint16_t readAnalog(uint8_t channel) override {
        select(channel);
        return analogRead(config_.signalPin);
    }

    bool supportsAnalog() const override { return true; }

private:
    Config config_;
    uint8_t current_channel_ = 0;
    bool initialized_ = false;
};

// ═══════════════════════════════════════════════════
// Pre-configured aliases
// ═══════════════════════════════════════════════════

using CD74HC4067 = GenericMux<4>;  // 16 channels
using CD74HC4051 = GenericMux<3>;  // 8 channels
using CD74HC4052 = GenericMux<2>;  // 4 channels

}  // namespace oc::drivers::arduino
```

### 6.3.2 - ButtonController.hpp

```cpp
#pragma once

#include <Arduino.h>
#include <array>
#include <oc/hal/IButtonController.hpp>
#include <oc/hal/IMultiplexer.hpp>
#include <oc/hal/Types.hpp>

namespace oc::drivers::arduino {

struct ButtonDef {
    hal::ButtonID id;
    hal::GpioPin pin;
    bool activeLow = true;  // Default = pull-up standard
};

template<size_t N>
class ButtonController : public hal::IButtonController {
public:
    ButtonController(
        const std::array<ButtonDef, N>& buttons,
        hal::IMultiplexer* mux = nullptr,
        uint8_t debounceMs = 5
    ) : buttons_(buttons), mux_(mux), debounce_ms_(debounceMs) {
        states_.fill(false);
        last_change_.fill(0);
    }

    bool init() override {
        for (const auto& btn : buttons_) {
            if (btn.pin.source == hal::GpioPin::Source::MCU) {
                pinMode(btn.pin.pin, INPUT_PULLUP);
            }
        }
        initialized_ = true;
        return true;
    }

    void update() override {
        if (!initialized_) return;
        
        uint32_t now = millis();
        
        for (size_t i = 0; i < N; ++i) {
            bool raw = readPin(buttons_[i]);
            bool pressed = buttons_[i].activeLow ? !raw : raw;
            
            if (pressed != states_[i]) {
                if (now - last_change_[i] >= debounce_ms_) {
                    states_[i] = pressed;
                    last_change_[i] = now;
                    
                    if (callback_) {
                        callback_(
                            buttons_[i].id,
                            pressed ? hal::ButtonEvent::Pressed 
                                    : hal::ButtonEvent::Released
                        );
                    }
                }
            }
        }
    }

    bool isPressed(hal::ButtonID id) const override {
        for (size_t i = 0; i < N; ++i) {
            if (buttons_[i].id == id) return states_[i];
        }
        return false;
    }

    void setCallback(hal::ButtonCallback cb) override {
        callback_ = cb;
    }

private:
    bool readPin(const ButtonDef& btn) {
        if (btn.pin.source == hal::GpioPin::Source::MCU) {
            return digitalRead(btn.pin.pin);
        } else {
            // MUX source - pin.pin is the channel number
            if (mux_) {
                return mux_->readDigital(btn.pin.pin);
            }
            return false;  // No mux configured
        }
    }

    std::array<ButtonDef, N> buttons_;
    hal::IMultiplexer* mux_;
    uint8_t debounce_ms_;
    
    std::array<bool, N> states_;
    std::array<uint32_t, N> last_change_;
    hal::ButtonCallback callback_;
    bool initialized_ = false;
};

}  // namespace oc::drivers::arduino
```

### 6.3.3 - EncoderController.hpp (Arduino générique)

**Dépendance** : `Encoder` (PJRC) - cross-platform

```cpp
#pragma once

#ifndef Encoder_h
#error "EncoderController requires Encoder library by PJRC. Add 'Encoder' to lib_deps"
#endif

#include <Arduino.h>
#include <Encoder.h>
#include <array>
#include <memory>
#include <oc/hal/IEncoderController.hpp>
#include <oc/hal/Types.hpp>

namespace oc::drivers::arduino {

struct EncoderDef {
    hal::EncoderID id;
    uint8_t pinA;
    uint8_t pinB;
    uint16_t ppr = 24;           // Pulses per revolution
    uint8_t stepsPerDetent = 4;  // Detent clicks per pulse
};

template<size_t N>
class EncoderController : public hal::IEncoderController {
public:
    explicit EncoderController(const std::array<EncoderDef, N>& defs)
        : defs_(defs) {
        modes_.fill(hal::EncoderMode::NORMALIZED);
        bounds_min_.fill(0.0f);
        bounds_max_.fill(1.0f);
        positions_.fill(0);
        last_values_.fill(0.5f);
    }

    bool init() override {
        // Lazy init - create Encoder objects here (not in constructor)
        // Reason: Arduino global objects are constructed before setup()
        for (size_t i = 0; i < N; ++i) {
            encoders_[i] = std::make_unique<Encoder>(
                defs_[i].pinA, 
                defs_[i].pinB
            );
        }
        initialized_ = true;
        return true;
    }

    void update() override {
        if (!initialized_) return;
        
        for (size_t i = 0; i < N; ++i) {
            int32_t pos = encoders_[i]->read();
            if (pos == positions_[i]) continue;
            
            int32_t delta = pos - positions_[i];
            positions_[i] = pos;
            
            float value = computeValue(i, pos, delta);
            
            if (value != last_values_[i]) {
                last_values_[i] = value;
                if (callback_) {
                    callback_(defs_[i].id, value);
                }
            }
        }
    }

    float getPosition(hal::EncoderID id) const override {
        for (size_t i = 0; i < N; ++i) {
            if (defs_[i].id == id) return last_values_[i];
        }
        return 0.0f;
    }

    void setPosition(hal::EncoderID id, float value) override {
        for (size_t i = 0; i < N; ++i) {
            if (defs_[i].id == id) {
                last_values_[i] = value;
                // Convert back to ticks for NORMALIZED mode
                if (modes_[i] == hal::EncoderMode::NORMALIZED) {
                    float range = bounds_max_[i] - bounds_min_[i];
                    float normalized = (value - bounds_min_[i]) / range;
                    positions_[i] = static_cast<int32_t>(
                        normalized * defs_[i].ppr * defs_[i].stepsPerDetent
                    );
                    if (encoders_[i]) encoders_[i]->write(positions_[i]);
                }
                return;
            }
        }
    }

    void setMode(hal::EncoderID id, hal::EncoderMode mode) override {
        for (size_t i = 0; i < N; ++i) {
            if (defs_[i].id == id) {
                modes_[i] = mode;
                return;
            }
        }
    }

    void setBounds(hal::EncoderID id, float min, float max) override {
        for (size_t i = 0; i < N; ++i) {
            if (defs_[i].id == id) {
                bounds_min_[i] = min;
                bounds_max_[i] = max;
                return;
            }
        }
    }

    void setDiscreteSteps(hal::EncoderID id, uint8_t steps) override {
        for (size_t i = 0; i < N; ++i) {
            if (defs_[i].id == id) {
                discrete_steps_[i] = steps;
                return;
            }
        }
    }

    void setContinuous(hal::EncoderID id) override {
        setDiscreteSteps(id, 0);
    }

    void setDelta(hal::EncoderID id, float delta) override {
        for (size_t i = 0; i < N; ++i) {
            if (defs_[i].id == id) {
                delta_per_detent_[i] = delta;
                return;
            }
        }
    }

    void setCallback(hal::EncoderCallback cb) override {
        callback_ = cb;
    }

private:
    float computeValue(size_t idx, int32_t pos, int32_t delta) {
        const auto& def = defs_[idx];
        
        switch (modes_[idx]) {
            case hal::EncoderMode::RAW:
                return static_cast<float>(pos);
                
            case hal::EncoderMode::RELATIVE: {
                // Delta per detent
                if (abs(delta) >= def.stepsPerDetent) {
                    return (delta > 0) ? delta_per_detent_[idx] 
                                       : -delta_per_detent_[idx];
                }
                return 0.0f;
            }
                
            case hal::EncoderMode::NORMALIZED:
            default: {
                // Position [0.0-1.0] based on PPR and bounds
                int32_t maxTicks = def.ppr * def.stepsPerDetent;
                pos = constrain(pos, 0, maxTicks);
                positions_[idx] = pos;  // Clamp stored position too
                
                float normalized = static_cast<float>(pos) / maxTicks;
                float range = bounds_max_[idx] - bounds_min_[idx];
                return bounds_min_[idx] + (normalized * range);
            }
        }
    }

    std::array<EncoderDef, N> defs_;
    std::array<std::unique_ptr<Encoder>, N> encoders_;
    
    std::array<hal::EncoderMode, N> modes_;
    std::array<float, N> bounds_min_;
    std::array<float, N> bounds_max_;
    std::array<int32_t, N> positions_;
    std::array<float, N> last_values_;
    std::array<uint8_t, N> discrete_steps_{};
    std::array<float, N> delta_per_detent_;
    
    hal::EncoderCallback callback_;
    bool initialized_ = false;
};

}  // namespace oc::drivers::arduino
```

### Validation Phase 6.3
- [ ] GenericMux compile avec aliases
- [ ] ButtonController compile avec MCU + MUX
- [ ] EncoderController compile avec lazy init
- [ ] `#error` si lib Encoder manquante

---

## Phase 6.4 : Drivers Teensy

### Structure

```
src/drivers/teensy/
├── input/
│   └── EncoderController.hpp/cpp   # EncoderTool optimisé
├── midi/
│   └── TeensyUsbMidi.hpp/cpp
└── display/
    └── Ili9341Driver.hpp/cpp
```

**Namespace** : `oc::drivers::teensy`

### 6.4.1 - TeensyUsbMidi

```cpp
#pragma once

#include <oc/hal/IMidiTransport.hpp>

namespace oc::drivers::teensy {

class TeensyUsbMidi : public hal::IMidiTransport {
public:
    bool init() override;
    void update() override;
    
    void sendCC(uint8_t channel, uint8_t cc, uint8_t value) override;
    void sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) override;
    void sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) override;
    void sendSysEx(const uint8_t* data, size_t length) override;
    void sendProgramChange(uint8_t channel, uint8_t program) override;
    void sendPitchBend(uint8_t channel, int16_t value) override;
    void sendChannelPressure(uint8_t channel, uint8_t pressure) override;
    void allNotesOff() override;
    
    void setOnCC(CCCallback cb) override { on_cc_ = cb; }
    void setOnNoteOn(NoteCallback cb) override { on_note_on_ = cb; }
    void setOnNoteOff(NoteCallback cb) override { on_note_off_ = cb; }
    void setOnSysEx(SysExCallback cb) override { on_sysex_ = cb; }

private:
    CCCallback on_cc_;
    NoteCallback on_note_on_;
    NoteCallback on_note_off_;
    SysExCallback on_sysex_;
};

}  // namespace oc::drivers::teensy
```

### 6.4.2 - EncoderController (Teensy optimisé)

Utilise `EncoderTool` (luni64) - optimisé Teensy avec callbacks ISR.

```cpp
#pragma once

#ifndef ENCODERTOOL_H
#error "Teensy EncoderController requires EncoderTool. Add 'luni64/EncoderTool' to lib_deps"
#endif

#include <EncoderTool.h>
#include <oc/hal/IEncoderController.hpp>

namespace oc::drivers::teensy {

// Similar structure to arduino version but using EncoderTool
// with ISR callbacks for better performance

}  // namespace oc::drivers::teensy
```

### 6.4.3 - Ili9341Driver

```cpp
#pragma once

#ifndef ILI9341_T4_H
#error "Ili9341Driver requires ILI9341_T4. Add 'ILI9341_T4' to lib_deps"
#endif

#include <oc/hal/IDisplayDriver.hpp>

namespace oc::drivers::teensy {

struct Ili9341Config {
    uint16_t width = 320;
    uint16_t height = 240;
    uint8_t csPin, dcPin, rstPin;
    uint8_t mosiPin, sckPin, misoPin;
    uint32_t spiSpeed = 20000000;
    uint8_t rotation = 3;
    uint8_t vsyncSpacing = 2;
};

class Ili9341Driver : public hal::IDisplayDriver {
public:
    explicit Ili9341Driver(const Ili9341Config& config);
    
    bool init() override;
    void flush(const void* buffer, const hal::Rect& area) override;
    uint16_t width() const override { return config_.width; }
    uint16_t height() const override { return config_.height; }

private:
    Ili9341Config config_;
    // ILI9341_T4 instance + DiffBuff internal
};

}  // namespace oc::drivers::teensy
```

### Validation Phase 6.4
- [ ] TeensyUsbMidi compile et utilise usbMIDI
- [ ] EncoderController Teensy compile avec EncoderTool
- [ ] Ili9341Driver compile avec ILI9341_T4
- [ ] `#error` si lib manquante

---

## Dépendances Externes

| Lib | Version | Driver | Board |
|-----|---------|--------|-------|
| `Encoder` | ^1.4.4 | `arduino::EncoderController` | Toutes |
| `luni64/EncoderTool` | ^2.2.0 | `teensy::EncoderController` | Teensy |
| `ILI9341_T4` | ^1.0.0 | `teensy::Ili9341Driver` | Teensy |

**Le framework n'a PAS ces dépendances** - elles sont optionnelles selon les drivers utilisés.

---

## Phase 7 : UI Optionnel (LVGL)

### Fichiers à créer

**`src/oc/ui/interface/IView.hpp`**
```cpp
#pragma once

namespace oc::ui {

class IView {
public:
    virtual ~IView() = default;
    virtual void onActivate() = 0;
    virtual void onDeactivate() = 0;
    virtual const char* getViewId() const = 0;
};

}  // namespace oc::ui
```

**`src/oc/ui/LVGLAdapter.hpp`**
```cpp
#pragma once
#ifdef OC_USE_LVGL

#include <lvgl.h>
#include <oc/core/struct/Binding.hpp>

namespace oc::ui {

inline core::IsActiveFn lvglIsActive(lv_obj_t* obj) {
    return [obj]() {
        return obj && !lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN);
    };
}

inline core::ScopeID lvglScopeID(lv_obj_t* obj) {
    return reinterpret_cast<core::ScopeID>(obj);
}

}  // namespace oc::ui

#endif
```

---

## Phase 8 : Example Minimal

### Structure

```
examples/minimal-teensy41/
├── src/main.cpp
├── include/Config.hpp
└── platformio.ini
```

### Critères de validation
- [ ] Compile sans erreur
- [ ] Upload sur Teensy 4.1
- [ ] Serial affiche "Ready!"
- [ ] Rotation encodeur → log
- [ ] Appui bouton → log

---

## Phases Futures (9-10)

### Phase 9 : Adapter midi-studio/core
- Ajouter `lib_deps = open-control`
- Supprimer code dupliqué (`adapter/`, `core/event/`, etc.)
- Créer `StandaloneContext`

### Phase 10 : Adapter context-bitwig
- Renommer `Plugin.hpp` → `BitwigContext.hpp`
- Changer héritage vers `oc::context::IContext`
- Ajouter `isConnected()`, `onDisconnected()`

---

# Chemins Repositories

| Repo | Chemin | Rôle |
|------|--------|------|
| **Framework** | `C:\Users\miu-lab\...\open-control\framework` | Cible |
| **Core (source)** | `C:\Users\miu-lab\...\midi-studio\core` | Référence (NE PAS MODIFIER) |
| **Bitwig (source)** | `C:\Users\miu-lab\...\midi-studio\plugin-bitwig` | Référence (NE PAS MODIFIER) |
