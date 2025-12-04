# Plan de Migration Open Control Framework

## Progression

| Phase | Statut | Date | Notes |
|-------|--------|------|-------|
| **0** | ✅ TERMINÉE | 2025-12-04 | Nettoyage repo, mise à jour config |
| **1** | ✅ TERMINÉE | 2025-12-04 | Structure + HAL Interfaces |
| **2** | ✅ TERMINÉE | 2025-12-04 | Core Logic (EventBus, InputBinding) |
| 3 | 🔴 À faire | - | Context System |
| 4 | 🔴 À faire | - | ControlAPI |
| 5 | 🔴 À faire | - | OpenControlApp + AppBuilder |
| 6 | 🔴 À faire | - | Drivers Teensy |
| 7 | 🔴 À faire | - | UI Optionnel |
| 8 | 🔴 À faire | - | Example Minimal |
| 9 | ⏸️ Futur | - | Adapter midi-studio/core |
| 10 | ⏸️ Futur | - | Adapter context-bitwig |

---

## Phase 0 : Nettoyage (TERMINÉE)

### Fichiers Supprimés
- `asset/font/` - Fonts spécifiques midi-studio
- `docs/` - Docs spécifiques (référentiel dans Serena)
- `script/midi/` - SysEx patch spécifique
- `script/pio/sysex_patch.py`
- `script/pio/pre_build.py`
- `script/pio/lib_pre_build.py`
- `script/pio/__pycache__/`

### Fichiers Mis à Jour
- `library.json` → open-control 0.1.0, Apache 2.0
- `platformio.ini` → minimal framework config
- `LICENSE` → Apache 2.0 (changé depuis CC-BY-NC-SA)
- `README.md` → Open Control Framework
- `pyproject.toml` → open-control-framework

### Fichiers Conservés
- `.clang-format`, `.clangd` - Config IDE
- `.vscode/` - IDE setup
- `script/lvgl/font/` - Font generation (générique)
- `script/lvgl/img/` - Image conversion (générique)
- `script/dev/` - Dev helpers
- `script/pio/compiledb_utils.py` - clangd support

### Note License
Plan original indiquait MIT, changé en **Apache 2.0** sur demande utilisateur.

---

## Chemins des Repositories

| Repo | Chemin Absolu | Rôle |
|------|---------------|------|
| **Framework (cible)** | `C:\Users\miu-lab\Documents\PlatformIO\Projects\petitechose.audio\open-control\framework` | Nouveau framework générique |
| **Core (source)** | `C:\Users\miu-lab\Documents\PlatformIO\Projects\petitechose.audio\midi-studio\core` | Code à migrer (NE PAS MODIFIER) |
| **Plugin Bitwig (source)** | `C:\Users\miu-lab\Documents\PlatformIO\Projects\petitechose.audio\midi-studio\plugin-bitwig` | Référence pour Context (NE PAS MODIFIER) |

---

## Principes de Migration

1. **COPIER, NE PAS MODIFIER** les sources originales
2. **Tester au plus tôt** via `examples/minimal-teensy41/`
3. **Ordre incrémental** : Framework → Example → Core adaptation → Context adaptation
4. **Chaque phase doit compiler** avant de passer à la suivante

---

## Architecture Cible Rappel

```
Namespace: oc::
├── hal/          # Interfaces hardware
├── core/         # EventBus, InputBinding, structs
├── context/      # IContext, ContextManager
├── api/          # ControlAPI
├── app/          # OpenControlApp, AppBuilder
├── ui/           # IView, LVGLManager (optionnel)
└── util/         # Log

drivers/teensy/   # Implémentations Teensy (hors namespace oc::)
```

---

# PHASE 1 : Structure + HAL Interfaces

## Objectif
Créer le squelette du framework et les interfaces abstraites HAL.

## Fichiers à Créer

### 1.1 Configuration Build

**`library.json`**
```json
{
  "name": "open-control",
  "version": "0.1.0",
  "description": "Open Control Framework for MIDI Controllers",
  "keywords": ["midi", "controller", "embedded", "lvgl"],
  "repository": {
    "type": "git",
    "url": "https://github.com/petitechose/open-control-framework"
  },
  "authors": [{"name": "petitechose.audio"}],
  "license": "MIT",
  "frameworks": ["arduino"],
  "platforms": ["teensy"]
}
```

**`platformio.ini`** (pour compilation lib)
```ini
[env:teensy41]
platform = teensy
board = teensy41
framework = arduino
build_flags = 
    -std=c++17
    -I src
```

### 1.2 Types de Base

**`src/oc/hal/Types.hpp`**
```cpp
#pragma once
#include <cstdint>
#include <functional>

namespace oc::hal {

// IDs génériques - le consommateur définit ses propres enums
using ButtonID = uint16_t;
using EncoderID = uint16_t;

// Événements bouton
enum class ButtonEvent : uint8_t { Pressed, Released };

// Rectangle pour display
struct Rect {
    int16_t x1, y1, x2, y2;
};

// Configuration GPIO
struct GpioPin {
    enum class Source : uint8_t { MCU, MUX };
    uint8_t pin;
    Source source = Source::MCU;
    bool activeLow = true;
};

// Callbacks
using ButtonCallback = std::function<void(ButtonID, ButtonEvent)>;
using EncoderCallback = std::function<void(EncoderID, int32_t position, int32_t delta)>;

} // namespace oc::hal
```

### 1.3 Interfaces HAL

**`src/oc/hal/IDisplayDriver.hpp`**
```cpp
#pragma once
#include "Types.hpp"

namespace oc::hal {

class IDisplayDriver {
public:
    virtual ~IDisplayDriver() = default;
    
    virtual void init() = 0;
    virtual void flush(const void* buffer, const Rect& area) = 0;
    virtual uint16_t width() const = 0;
    virtual uint16_t height() const = 0;
    
    // Callback quand flush terminé (pour LVGL)
    using FlushCallback = void(*)(IDisplayDriver*);
    virtual void setFlushCallback(FlushCallback cb) { flush_cb_ = cb; }
    
protected:
    FlushCallback flush_cb_ = nullptr;
};

} // namespace oc::hal
```

**`src/oc/hal/IMidiTransport.hpp`**
```cpp
#pragma once
#include <cstdint>
#include <cstddef>
#include <functional>

namespace oc::hal {

class IMidiTransport {
public:
    virtual ~IMidiTransport() = default;
    
    // Lifecycle
    virtual void init() = 0;
    virtual void update() = 0;  // Process incoming messages
    
    // Output
    virtual void sendCC(uint8_t channel, uint8_t cc, uint8_t value) = 0;
    virtual void sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) = 0;
    virtual void sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) = 0;
    virtual void sendSysEx(const uint8_t* data, size_t length) = 0;
    virtual void sendProgramChange(uint8_t channel, uint8_t program) = 0;
    virtual void sendPitchBend(uint8_t channel, int16_t value) = 0;
    
    // Input callbacks
    using CCCallback = std::function<void(uint8_t ch, uint8_t cc, uint8_t val)>;
    using NoteCallback = std::function<void(uint8_t ch, uint8_t note, uint8_t vel)>;
    using SysExCallback = std::function<void(const uint8_t* data, size_t len)>;
    
    virtual void setOnCC(CCCallback cb) = 0;
    virtual void setOnNoteOn(NoteCallback cb) = 0;
    virtual void setOnNoteOff(NoteCallback cb) = 0;
    virtual void setOnSysEx(SysExCallback cb) = 0;
};

} // namespace oc::hal
```

**`src/oc/hal/IEncoderController.hpp`**
```cpp
#pragma once
#include "Types.hpp"

namespace oc::hal {

enum class EncoderMode : uint8_t { 
    ABSOLUTE,   // Position 0-max
    RELATIVE    // Delta seulement
};

class IEncoderController {
public:
    virtual ~IEncoderController() = default;
    
    virtual void init() = 0;
    virtual void update() = 0;
    
    // State
    virtual int32_t getPosition(EncoderID id) const = 0;
    virtual void setPosition(EncoderID id, int32_t position) = 0;
    
    // Configuration
    virtual void setMode(EncoderID id, EncoderMode mode) = 0;
    virtual void setBounds(EncoderID id, int32_t min, int32_t max) = 0;
    virtual void setDiscreteSteps(EncoderID id, uint8_t steps) = 0;
    virtual void setContinuous(EncoderID id) = 0;
    
    // Callback
    virtual void setCallback(EncoderCallback cb) = 0;
};

} // namespace oc::hal
```

**`src/oc/hal/IButtonController.hpp`**
```cpp
#pragma once
#include "Types.hpp"

namespace oc::hal {

class IButtonController {
public:
    virtual ~IButtonController() = default;
    
    virtual void init() = 0;
    virtual void update() = 0;
    
    virtual bool isPressed(ButtonID id) const = 0;
    virtual void setCallback(ButtonCallback cb) = 0;
};

} // namespace oc::hal
```

**`src/oc/hal/IMultiplexer.hpp`**
```cpp
#pragma once
#include <cstdint>

namespace oc::hal {

class IMultiplexer {
public:
    virtual ~IMultiplexer() = default;
    
    virtual void init() = 0;
    virtual void select(uint8_t channel) = 0;
    virtual bool readChannel(uint8_t channel) = 0;
    virtual uint8_t channelCount() const = 0;
};

} // namespace oc::hal
```

### 1.4 Validation Phase 1

```bash
cd C:\Users\miu-lab\Documents\PlatformIO\Projects\petitechose.audio\open-control\framework
pio run -e teensy41
```

**Critère** : Compilation sans erreur (lib vide avec headers only)

---

# PHASE 2 : Core Logic (EventBus, InputBinding, Structs)

## Objectif
Migrer la logique métier sans dépendances hardware.

## Fichiers à Copier et Adapter

### 2.1 Source → Destination

| Source (core) | Destination (framework) | Adaptations |
|---------------|------------------------|-------------|
| `core/event/Event.hpp` | `src/oc/core/event/Event.hpp` | Namespace `oc::core::event` |
| `core/event/IEventBus.hpp` | `src/oc/core/event/IEventBus.hpp` | Namespace |
| `core/event/EventBus.hpp` | `src/oc/core/event/EventBus.hpp` | Namespace |
| `core/event/Events.hpp` | `src/oc/core/event/Events.hpp` | Namespace |
| `core/event/UnifiedEventTypes.hpp` | `src/oc/core/event/EventTypes.hpp` | Namespace, renommer |
| `core/struct/Binding.hpp` | `src/oc/core/struct/Binding.hpp` | Supprimer `lv_obj_t*`, utiliser `VisibilityPredicate` |
| `core/struct/Button.hpp` | `src/oc/core/struct/Button.hpp` | Namespace |
| `core/struct/Encoder.hpp` | `src/oc/core/struct/Encoder.hpp` | Namespace |
| `core/struct/MidiCCMapping.hpp` | `src/oc/core/struct/MidiMapping.hpp` | Namespace |
| `core/input/InputBinding.hpp` | `src/oc/core/input/InputBinding.hpp` | Supprimer LVGL, utiliser `VisibilityPredicate` |
| `core/input/InputBinding.cpp` | `src/oc/core/input/InputBinding.cpp` | Supprimer `lv_obj_has_flag` |

### 2.2 Nouveau Fichier : InputConfig

**`src/oc/core/input/InputConfig.hpp`**
```cpp
#pragma once
#include <cstdint>

namespace oc::core {

struct InputConfig {
    uint32_t longPressMs = 500;
    uint32_t doubleTapWindowMs = 300;
    uint32_t latchThresholdMs = 300;
    uint32_t debounceMs = 5;
};

// Default global instance
inline InputConfig defaultInputConfig;

} // namespace oc::core
```

### 2.3 Binding.hpp Adapté (sans LVGL)

**`src/oc/core/struct/Binding.hpp`**
```cpp
#pragma once
#include <cstdint>
#include <functional>
#include <optional>
#include <oc/hal/Types.hpp>

namespace oc::core {

using VisibilityPredicate = std::function<bool()>;
using ScopeId = uintptr_t;
using ActionCallback = std::function<void()>;
using EncoderActionCallback = std::function<void(float)>;

enum class ButtonBindingType : uint8_t { 
    PRESS, RELEASE, LONG_PRESS, DOUBLE_TAP, COMBO 
};

enum class EncoderBindingType : uint8_t { 
    TURN, TURN_WHILE_PRESSED 
};

struct ButtonBinding {
    ButtonBindingType type;
    hal::ButtonID buttonId;
    std::optional<hal::ButtonID> secondaryButton;  // Pour COMBO
    uint32_t longPressMs = 0;  // 0 = use default
    ActionCallback action;
    bool enabled = true;
    bool latch = false;
    
    // Scope abstrait (sans LVGL)
    VisibilityPredicate isVisible = []() { return true; };
    ScopeId scopeId = 0;  // 0 = global
};

struct EncoderBinding {
    EncoderBindingType type;
    hal::EncoderID encoderId;
    std::optional<hal::ButtonID> requiredButton;  // Pour TURN_WHILE_PRESSED
    EncoderActionCallback action;
    bool enabled = true;
    
    VisibilityPredicate isVisible = []() { return true; };
    ScopeId scopeId = 0;
};

} // namespace oc::core
```

### 2.4 Validation Phase 2

- EventBus doit compiler
- InputBinding doit compiler SANS `#include <lvgl.h>`
- Test unitaire EventBus (subscribe/emit) si possible

---

# PHASE 3 : Context System

## Objectif
Créer le système de contextes (IContext, ContextManager).

### 3.1 Fichiers à Créer

**`src/oc/context/IContext.hpp`**
```cpp
#pragma once

namespace oc {
class ControlAPI;  // Forward
}

namespace oc::context {

class IContext {
public:
    virtual ~IContext() = default;
    
    // Lifecycle
    virtual bool initialize(ControlAPI& api) = 0;
    virtual void update() = 0;
    virtual void cleanup() = 0;
    
    // Identity
    virtual const char* getName() const = 0;
    virtual const char* getId() const = 0;
    
    // Connection state (for DAW integrations)
    virtual bool isConnected() const { return true; }
    virtual void onConnected() {}
    virtual void onDisconnected() {}
    
    // Resource loading (optionnel, appelé avant initialize)
    // Implémenté via SFINAE dans ContextManager
};

} // namespace oc::context
```

**`src/oc/context/ContextManager.hpp`**
```cpp
#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <type_traits>
#include "IContext.hpp"

namespace oc {
class ControlAPI;
}

namespace oc::context {

// SFINAE helper pour détecter loadResources()
template<typename T, typename = void>
struct has_load_resources : std::false_type {};

template<typename T>
struct has_load_resources<T, std::void_t<decltype(T::loadResources())>> 
    : std::true_type {};

class ContextManager {
public:
    explicit ContextManager(ControlAPI& api) : api_(api) {}
    
    template<typename T>
    bool registerContext(const std::string& id) {
        static_assert(std::is_base_of_v<IContext, T>,
                      "T must inherit from IContext");
        
        if (contexts_.find(id) != contexts_.end()) {
            return false;  // Already registered
        }
        
        // Load resources if available (SFINAE)
        if constexpr (has_load_resources<T>::value) {
            T::loadResources();
        }
        
        auto ctx = std::make_unique<T>();
        if (!ctx->initialize(api_)) {
            return false;
        }
        
        contexts_[id] = std::move(ctx);
        
        // First context becomes default
        if (default_id_.empty()) {
            default_id_ = id;
        }
        
        return true;
    }
    
    bool switchTo(const std::string& id) {
        auto it = contexts_.find(id);
        if (it == contexts_.end()) return false;
        
        if (active_) {
            active_->onDisconnected();
        }
        
        active_ = it->second.get();
        active_->onConnected();
        return true;
    }
    
    void switchToDefault() {
        if (!default_id_.empty()) {
            switchTo(default_id_);
        }
    }
    
    void setDefault(const std::string& id) {
        default_id_ = id;
    }
    
    IContext* active() const { return active_; }
    
    void update() {
        if (active_) {
            active_->update();
        }
    }
    
private:
    ControlAPI& api_;
    std::unordered_map<std::string, std::unique_ptr<IContext>> contexts_;
    IContext* active_ = nullptr;
    std::string default_id_;
};

} // namespace oc::context
```

### 3.2 Validation Phase 3

- IContext et ContextManager compilent
- Template registerContext<T> fonctionne

---

# PHASE 4 : ControlAPI

## Objectif
Créer la façade API pour les contextes.

### 4.1 Fichiers à Créer

**`src/oc/api/ControlAPI.hpp`**
```cpp
#pragma once
#include <oc/hal/Types.hpp>
#include <oc/hal/IMidiTransport.hpp>
#include <oc/hal/IEncoderController.hpp>
#include <oc/core/input/InputBinding.hpp>
#include <oc/core/event/IEventBus.hpp>

namespace oc {

// Forward declarations
namespace hal {
    class IDisplayDriver;
    class IButtonController;
}

class ControlAPI {
public:
    ControlAPI(
        core::input::InputBinding& binding,
        core::event::IEventBus& eventBus,
        hal::IMidiTransport& midi,
        hal::IEncoderController& encoders
    );
    
    // ═══════════════════════════════════════════════════
    // INPUT BINDING - Global (always active)
    // ═══════════════════════════════════════════════════
    
    void onPressed(hal::ButtonID id, core::ActionCallback cb);
    void onReleased(hal::ButtonID id, core::ActionCallback cb);
    void onLongPress(hal::ButtonID id, core::ActionCallback cb, uint32_t ms = 0);
    void onDoubleTap(hal::ButtonID id, core::ActionCallback cb);
    void onCombo(hal::ButtonID btn1, hal::ButtonID btn2, core::ActionCallback cb);
    
    void onTurned(hal::EncoderID id, core::EncoderActionCallback cb);
    void onTurnedWhilePressed(hal::EncoderID enc, hal::ButtonID btn, 
                              core::EncoderActionCallback cb);
    
    // ═══════════════════════════════════════════════════
    // INPUT BINDING - Scoped (active when scope visible)
    // ═══════════════════════════════════════════════════
    
    void onPressed(hal::ButtonID id, core::ActionCallback cb,
                   core::VisibilityPredicate isVisible, core::ScopeId scope,
                   bool latch = false);
    void onReleased(hal::ButtonID id, core::ActionCallback cb,
                    core::VisibilityPredicate isVisible, core::ScopeId scope);
    void onLongPress(hal::ButtonID id, core::ActionCallback cb, uint32_t ms,
                     core::VisibilityPredicate isVisible, core::ScopeId scope);
    void onDoubleTap(hal::ButtonID id, core::ActionCallback cb,
                     core::VisibilityPredicate isVisible, core::ScopeId scope);
    void onCombo(hal::ButtonID btn1, hal::ButtonID btn2, core::ActionCallback cb,
                 core::VisibilityPredicate isVisible, core::ScopeId scope);
    
    void onTurned(hal::EncoderID id, core::EncoderActionCallback cb,
                  core::VisibilityPredicate isVisible, core::ScopeId scope);
    void onTurnedWhilePressed(hal::EncoderID enc, hal::ButtonID btn,
                              core::EncoderActionCallback cb,
                              core::VisibilityPredicate isVisible, core::ScopeId scope);
    
    void clearScope(core::ScopeId scope);
    
    // Latch state
    bool isLatched(hal::ButtonID btn) const;
    void setLatch(hal::ButtonID btn, bool latched);
    
    // ═══════════════════════════════════════════════════
    // ENCODER CONTROL
    // ═══════════════════════════════════════════════════
    
    void setEncoderPosition(hal::EncoderID id, float normalizedValue);
    void setEncoderMode(hal::EncoderID id, hal::EncoderMode mode);
    void setEncoderBounds(hal::EncoderID id, float min, float max);
    void setEncoderDiscreteSteps(hal::EncoderID id, uint8_t steps);
    void setEncoderContinuous(hal::EncoderID id);
    
    // ═══════════════════════════════════════════════════
    // MIDI OUTPUT
    // ═══════════════════════════════════════════════════
    
    void sendCC(uint8_t channel, uint8_t cc, uint8_t value);
    void sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity);
    void sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity);
    void sendSysEx(const uint8_t* data, size_t length);
    
    // ═══════════════════════════════════════════════════
    // MIDI INPUT (Templates for callbacks)
    // ═══════════════════════════════════════════════════
    
    template<typename Callback>
    void onCC(Callback cb);
    
    template<typename Callback>
    void onNoteOn(Callback cb);
    
    template<typename Callback>
    void onNoteOff(Callback cb);
    
    template<typename Callback>
    void onSysEx(Callback cb);
    
    // ═══════════════════════════════════════════════════
    // LOGGING
    // ═══════════════════════════════════════════════════
    
    void log(const char* message);
    
    template<typename... Args>
    void logf(const char* format, Args... args);
    
private:
    core::input::InputBinding& binding_;
    core::event::IEventBus& eventBus_;
    hal::IMidiTransport& midi_;
    hal::IEncoderController& encoders_;
};

} // namespace oc
```

### 4.2 Validation Phase 4

- ControlAPI compile
- Toutes les méthodes délèguent correctement

---

# PHASE 5 : OpenControlApp + AppBuilder

## Objectif
Créer l'application principale et le builder.

### 5.1 Fichiers à Créer

**`src/oc/app/AppBuilder.hpp`**
```cpp
#pragma once
#include <memory>
#include <oc/hal/IDisplayDriver.hpp>
#include <oc/hal/IMidiTransport.hpp>
#include <oc/hal/IEncoderController.hpp>
#include <oc/hal/IButtonController.hpp>
#include <oc/core/input/InputConfig.hpp>

namespace oc::app {

class OpenControlApp;

class AppBuilder {
public:
    AppBuilder& display(std::unique_ptr<hal::IDisplayDriver> driver);
    AppBuilder& midi(std::unique_ptr<hal::IMidiTransport> transport);
    AppBuilder& encoders(std::unique_ptr<hal::IEncoderController> controller);
    AppBuilder& buttons(std::unique_ptr<hal::IButtonController> controller);
    AppBuilder& inputConfig(const core::InputConfig& config);
    
    OpenControlApp build();
    
private:
    std::unique_ptr<hal::IDisplayDriver> display_;
    std::unique_ptr<hal::IMidiTransport> midi_;
    std::unique_ptr<hal::IEncoderController> encoders_;
    std::unique_ptr<hal::IButtonController> buttons_;
    core::InputConfig inputConfig_;
};

} // namespace oc::app
```

**`src/oc/app/OpenControlApp.hpp`**
```cpp
#pragma once
#include <memory>
#include <oc/hal/IDisplayDriver.hpp>
#include <oc/hal/IMidiTransport.hpp>
#include <oc/hal/IEncoderController.hpp>
#include <oc/hal/IButtonController.hpp>
#include <oc/core/event/EventBus.hpp>
#include <oc/core/input/InputBinding.hpp>
#include <oc/context/ContextManager.hpp>
#include <oc/api/ControlAPI.hpp>

namespace oc::app {

class AppBuilder;

class OpenControlApp {
public:
    friend class AppBuilder;
    
    void begin();
    void update();
    
    // Accessors
    ControlAPI& api() { return *api_; }
    context::ContextManager& contexts() { return *contexts_; }
    core::event::IEventBus& eventBus() { return eventBus_; }
    
    // Context registration shortcut
    template<typename T>
    bool registerContext(const std::string& id) {
        return contexts_->registerContext<T>(id);
    }
    
private:
    OpenControlApp() = default;
    
    // Hardware (owned)
    std::unique_ptr<hal::IDisplayDriver> display_;
    std::unique_ptr<hal::IMidiTransport> midi_;
    std::unique_ptr<hal::IEncoderController> encoders_;
    std::unique_ptr<hal::IButtonController> buttons_;
    
    // Core services
    core::event::EventBus eventBus_;
    std::unique_ptr<core::input::InputBinding> inputBinding_;
    
    // API and context
    std::unique_ptr<ControlAPI> api_;
    std::unique_ptr<context::ContextManager> contexts_;
    
    // Config
    core::InputConfig inputConfig_;
};

} // namespace oc::app
```

### 5.2 Validation Phase 5

- AppBuilder et OpenControlApp compilent
- Builder pattern fonctionne

---

# PHASE 6 : Drivers Teensy

## Objectif
Implémenter les interfaces HAL pour Teensy 4.x.

### 6.1 Fichiers à Copier et Adapter

| Source (core) | Destination (framework) | Adaptations |
|---------------|------------------------|-------------|
| `adapter/display/driver/Ili9341Driver.hpp` | `src/drivers/teensy/display/Ili9341Driver.hpp` | Implémenter `IDisplayDriver` |
| `adapter/display/driver/Ili9341Driver.cpp` | `src/drivers/teensy/display/Ili9341Driver.cpp` | Config via struct |
| `adapter/display/ui/LVGLBridge.hpp` | `src/drivers/teensy/display/LVGLTeensyBridge.hpp` | Adapter |
| `adapter/display/ui/LVGLBridge.cpp` | `src/drivers/teensy/display/LVGLTeensyBridge.cpp` | |
| `adapter/display/ui/LVGLMemory.hpp` | `src/drivers/teensy/display/LVGLMemory.hpp` | |
| `adapter/input/encoder/Encoder.hpp` | `src/drivers/teensy/input/TeensyEncoder.hpp` | |
| `adapter/input/encoder/Encoder.cpp` | `src/drivers/teensy/input/TeensyEncoder.cpp` | |
| `adapter/input/encoder/EncoderController.hpp` | `src/drivers/teensy/input/TeensyEncoderController.hpp` | Template, implémenter `IEncoderController` |
| `adapter/input/encoder/EncoderController.cpp` | `src/drivers/teensy/input/TeensyEncoderController.cpp` | |
| `adapter/input/button/ButtonController.hpp` | `src/drivers/teensy/input/TeensyButtonController.hpp` | Template, implémenter `IButtonController` |
| `adapter/input/button/ButtonController.cpp` | `src/drivers/teensy/input/TeensyButtonController.cpp` | |
| `adapter/input/button/ButtonFactory.hpp` | (intégré dans TeensyButtonController) | |
| `adapter/input/button/UnifiedButton.hpp` | `src/drivers/teensy/input/TeensyButton.hpp` | |
| `adapter/input/button/reader/*` | `src/drivers/teensy/input/reader/*` | |
| `adapter/multiplexer/MultiplexerController.hpp` | `src/drivers/teensy/input/CD74HC4067.hpp` | Implémenter `IMultiplexer` |
| `adapter/multiplexer/MultiplexerController.cpp` | `src/drivers/teensy/input/CD74HC4067.cpp` | Config via struct |
| `adapter/midi/TeensyUsbMidiIn.hpp` | `src/drivers/teensy/midi/TeensyUsbMidi.hpp` | Fusionner In+Out, implémenter `IMidiTransport` |
| `adapter/midi/TeensyUsbMidiIn.cpp` | `src/drivers/teensy/midi/TeensyUsbMidi.cpp` | |
| `adapter/midi/TeensyUsbMidiOut.hpp` | (fusionné) | |
| `adapter/midi/TeensyUsbMidiOut.cpp` | (fusionné) | |

### 6.2 Structures de Configuration

**`src/drivers/teensy/display/Ili9341Driver.hpp`** (extrait)
```cpp
namespace oc::drivers::teensy {

struct Ili9341Config {
    uint16_t width = 320;
    uint16_t height = 240;
    uint8_t cs_pin, dc_pin, rst_pin, mosi_pin, sck_pin, miso_pin;
    uint32_t spi_speed = 20000000;
    uint8_t rotation = 3;
};

class Ili9341Driver : public hal::IDisplayDriver {
public:
    explicit Ili9341Driver(const Ili9341Config& config);
    // ... implémentation IDisplayDriver
};

}
```

**`src/drivers/teensy/input/CD74HC4067.hpp`** (extrait)
```cpp
namespace oc::drivers::teensy {

struct CD74HC4067Config {
    uint8_t s0_pin, s1_pin, s2_pin, s3_pin;
    uint8_t sig_pin;
    uint32_t debounce_us = 20;
};

class CD74HC4067 : public hal::IMultiplexer {
public:
    explicit CD74HC4067(const CD74HC4067Config& config);
    // ... implémentation IMultiplexer
};

}
```

### 6.3 Validation Phase 6

- Tous les drivers compilent
- Chaque driver implémente son interface HAL

---

# PHASE 7 : UI Optionnel (LVGL)

## Objectif
Créer les composants UI optionnels.

### 7.1 Fichiers à Créer/Copier

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

} // namespace oc::ui
```

**`src/oc/ui/LVGLAdapter.hpp`** (pour scoped bindings avec LVGL)
```cpp
#pragma once
#ifdef OC_USE_LVGL

#include <lvgl.h>
#include <oc/core/struct/Binding.hpp>

namespace oc::ui {

// Helper pour créer VisibilityPredicate depuis lv_obj_t*
inline core::VisibilityPredicate lvglVisibility(lv_obj_t* obj) {
    return [obj]() {
        return obj && !lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN);
    };
}

// Helper pour convertir lv_obj_t* en ScopeId
inline core::ScopeId lvglScopeId(lv_obj_t* obj) {
    return reinterpret_cast<core::ScopeId>(obj);
}

} // namespace oc::ui

#endif // OC_USE_LVGL
```

### 7.2 Widgets à Copier (optionnel, pour plus tard)

| Source (core) | Destination (framework) |
|---------------|------------------------|
| `ui/shared/widget/Label.hpp` | `src/oc/ui/widget/Label.hpp` |
| `ui/shared/widget/ButtonIndicator.hpp` | `src/oc/ui/widget/ButtonIndicator.hpp` |
| ... | ... |

### 7.3 Validation Phase 7

- IView compile
- LVGLAdapter compile si `OC_USE_LVGL` défini

---

# PHASE 8 : Example Minimal

## Objectif
Créer un exemple testable pour valider le framework.

### 8.1 Structure

```
examples/minimal-teensy41/
├── src/
│   └── main.cpp
├── include/
│   └── Config.hpp
└── platformio.ini
```

### 8.2 Fichiers

**`examples/minimal-teensy41/platformio.ini`**
```ini
[env:teensy41]
platform = teensy
board = teensy41
framework = arduino
board_build.f_cpu = 450000000L

build_flags = 
    -std=c++17
    -D USB_MIDI_SERIAL
    -D OC_USE_LVGL
    -D LV_CONF_INCLUDE_SIMPLE
    -I ../../src
    -I include

lib_deps = 
    lvgl/lvgl @ ^9.2.0
    ILI9341_T4 @ ^1.0.0
    luni64/EncoderTool @ ^2.2.0
```

**`examples/minimal-teensy41/include/Config.hpp`**
```cpp
#pragma once
#include <drivers/teensy/display/Ili9341Driver.hpp>
#include <drivers/teensy/input/TeensyEncoderController.hpp>
#include <drivers/teensy/input/TeensyButtonController.hpp>
#include <drivers/teensy/midi/TeensyUsbMidi.hpp>

namespace Config {

// IDs locaux
enum class Button : oc::hal::ButtonID {
    BTN_1 = 1,
    BTN_2 = 2
};

enum class Encoder : oc::hal::EncoderID {
    ENC_1 = 1,
    ENC_2 = 2
};

// Display config
constexpr oc::drivers::teensy::Ili9341Config DISPLAY = {
    .width = 320,
    .height = 240,
    .cs_pin = 10,
    .dc_pin = 9,
    .rst_pin = 8,
    .mosi_pin = 11,
    .sck_pin = 13,
    .miso_pin = 12,
    .spi_speed = 20000000,
    .rotation = 3
};

// Encoders (2 seulement)
constexpr std::array ENCODERS = {
    oc::drivers::teensy::EncoderDef{
        static_cast<oc::hal::EncoderID>(Encoder::ENC_1), 2, 3
    },
    oc::drivers::teensy::EncoderDef{
        static_cast<oc::hal::EncoderID>(Encoder::ENC_2), 4, 5
    }
};

// Buttons (2 seulement, GPIO direct, PAS de mux)
constexpr std::array BUTTONS = {
    oc::drivers::teensy::ButtonDef{
        static_cast<oc::hal::ButtonID>(Button::BTN_1), 
        6, 
        oc::hal::GpioPin::Source::MCU
    },
    oc::drivers::teensy::ButtonDef{
        static_cast<oc::hal::ButtonID>(Button::BTN_2), 
        7, 
        oc::hal::GpioPin::Source::MCU
    }
};

} // namespace Config
```

**`examples/minimal-teensy41/src/main.cpp`**
```cpp
#include <Arduino.h>
#include <oc/app/AppBuilder.hpp>
#include <Config.hpp>

using namespace oc;
using namespace oc::drivers::teensy;

std::unique_ptr<app::OpenControlApp> app;

// Context minimal pour test
class MinimalContext : public context::IContext {
public:
    bool initialize(ControlAPI& api) override {
        api_ = &api;
        
        // Log encoder turns
        api.onTurned(static_cast<hal::EncoderID>(Config::Encoder::ENC_1),
            [](float delta) {
                Serial.printf("Encoder 1: delta=%.2f\n", delta);
            });
        
        api.onTurned(static_cast<hal::EncoderID>(Config::Encoder::ENC_2),
            [](float delta) {
                Serial.printf("Encoder 2: delta=%.2f\n", delta);
            });
        
        // Log button presses
        api.onPressed(static_cast<hal::ButtonID>(Config::Button::BTN_1),
            []() { Serial.println("Button 1 pressed"); });
        
        api.onReleased(static_cast<hal::ButtonID>(Config::Button::BTN_1),
            []() { Serial.println("Button 1 released"); });
        
        api.onPressed(static_cast<hal::ButtonID>(Config::Button::BTN_2),
            []() { Serial.println("Button 2 pressed"); });
        
        return true;
    }
    
    void update() override {}
    void cleanup() override {}
    
    const char* getName() const override { return "Minimal"; }
    const char* getId() const override { return "minimal"; }
    
private:
    ControlAPI* api_ = nullptr;
};

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("=== Open Control - Minimal Example ===");
    
    app = std::make_unique<app::OpenControlApp>(
        app::AppBuilder()
            .display(std::make_unique<Ili9341Driver>(Config::DISPLAY))
            .midi(std::make_unique<TeensyUsbMidi>())
            .encoders(std::make_unique<TeensyEncoderController<2>>(Config::ENCODERS))
            .buttons(std::make_unique<TeensyButtonController<2>>(Config::BUTTONS))
            .build()
    );
    
    app->registerContext<MinimalContext>("minimal");
    app->contexts().switchTo("minimal");
    
    app->begin();
    
    Serial.println("Ready! Turn encoders or press buttons.");
}

void loop() {
    app->update();
}
```

### 8.3 Validation Phase 8

```bash
cd examples/minimal-teensy41
pio run
pio run -t upload
```

**Critères de validation :**
- [ ] Compile sans erreur
- [ ] Upload sur Teensy 4.1
- [ ] Serial affiche "Ready!"
- [ ] Rotation encodeur → log "Encoder X: delta=..."
- [ ] Appui bouton → log "Button X pressed/released"

---

# PHASE 9 : Adaptation midi-studio/core (FUTUR)

## Objectif
Transformer midi-studio/core en consommateur du framework.

### 9.1 Modifications

1. **platformio.ini** : Ajouter `lib_deps = open-control/framework`
2. **Supprimer** : `adapter/`, `core/event/`, `core/input/`, `core/struct/`
3. **Garder** : `config/`, `boot/`, `ui/` (ViewManager, SplashView)
4. **Créer** : `context/StandaloneContext.hpp`
5. **Adapter** : `main.cpp` pour utiliser AppBuilder

### 9.2 Nouvelle structure midi-studio/core

```
midi-studio/core/
├── src/
│   ├── config/
│   │   ├── Hardware.hpp         # Pins, config ILI9341, mux, etc.
│   │   ├── InputIDs.hpp         # enum class Button, Encoder
│   │   └── MidiMappings.hpp
│   ├── context/
│   │   ├── StandaloneContext.hpp
│   │   └── StandaloneContext.cpp
│   ├── boot/
│   │   └── BootSequence.hpp     # Boot spécifique
│   ├── ui/
│   │   ├── ViewManager.hpp      # Gestion écrans
│   │   └── SplashView.hpp
│   └── main.cpp
└── platformio.ini
```

---

# PHASE 10 : Adaptation context-bitwig (FUTUR)

## Objectif
Transformer plugin-bitwig en Context.

### 10.1 Modifications

1. **Renommer** : `Plugin.hpp` → `BitwigContext.hpp`
2. **Changer héritage** : `IPlugin` → `oc::context::IContext`
3. **Ajouter méthodes** : `isConnected()`, `onDisconnected()`
4. **Adapter main.cpp** : Utiliser ContextManager
5. **Garder intact** : `protocol/`, `handler/`, `ui/` (interne)

### 10.2 BitwigContext.hpp (aperçu)

```cpp
namespace MidiStudio::Context {

class Bitwig : public oc::context::IContext {
public:
    static void loadResources();  // SFINAE preserved
    
    bool initialize(oc::ControlAPI& api) override;
    void update() override;
    void cleanup() override;
    
    const char* getName() const override { return "Bitwig Studio"; }
    const char* getId() const override { return "bitwig"; }
    
    bool isConnected() const override { return protocol_.isConnected(); }
    void onDisconnected() override;
    
private:
    // Tout le reste identique à Plugin actuel
    oc::ControlAPI* api_;
    Protocol::Protocol protocol_;
    // handlers, views, etc.
};

}
```

---

# Récapitulatif Phases

| Phase | Objectif | Validation |
|-------|----------|------------|
| 1 | Structure + HAL interfaces | `pio run` compile |
| 2 | Core logic (EventBus, InputBinding) | Compile sans LVGL |
| 3 | Context system | IContext, ContextManager compilent |
| 4 | ControlAPI | API compile |
| 5 | OpenControlApp + Builder | Builder fonctionne |
| 6 | Drivers Teensy | Tous drivers compilent |
| 7 | UI optionnel | IView, LVGLAdapter compilent |
| 8 | Example minimal | **TEST HARDWARE** ✅ |
| 9 | Adapter midi-studio/core | (Futur) |
| 10 | Adapter context-bitwig | (Futur) |

---

# Estimation Temps

| Phase | Durée |
|-------|-------|
| 1-2 | 1 jour |
| 3-4 | 0.5 jour |
| 5 | 0.5 jour |
| 6 | 1.5 jours |
| 7 | 0.5 jour |
| 8 | 0.5 jour |
| **Total Framework** | **4.5 jours** |
| 9 | 1 jour |
| 10 | 0.5 jour |
| **Total Complet** | **6 jours** |
