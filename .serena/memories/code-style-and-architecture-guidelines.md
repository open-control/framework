# Code Style & Architecture Guidelines - Open Control Framework

## Référentiel

Ce document est le **référentiel officiel** des conventions de code et d'architecture pour le framework Open Control.
Il est basé sur les guidelines de midi-studio/core avec les adaptations mineures pour le framework générique.

**Source originale** : `C:\Users\miu-lab\Documents\PlatformIO\Projects\petitechose.audio\midi-studio\core\docs\`

---

## Adaptations Mineures par Rapport à Core

| Aspect | Core Original | Framework Open Control |
|--------|---------------|------------------------|
| Namespace global | Aucun | `oc::` avec sous-namespaces |
| Interface plugins | `IPlugin` | `IContext` (+ méthodes `isConnected()`, `onDisconnected()`) |
| API principale | `ControllerAPI` | `ControlAPI` |
| Boot sequence | `BootManager` dans core | Dans consommateur |
| ViewManager | Dans core | Dans consommateur |
| Référence MIDI Studio | Partout | Généralisé "Open Control" |

---

# ARCHITECTURE

## Overview

Le framework suit une **Clean Architecture** adaptée pour systèmes embarqués, avec séparation claire des responsabilités :

```
┌─────────────────────────────────────────────────────────────┐
│                      main.cpp                               │  Entry Point
├─────────────────────────────────────────────────────────────┤
│                    OpenControlApp                           │  Composition Root
├─────────────────────────────────────────────────────────────┤
│              ContextManager + ControlAPI                    │  Orchestration
├─────────────────────────────────────────────────────────────┤
│     Core (EventBus, InputBinding, Types)                    │  Business Logic
├─────────────────────────────────────────────────────────────┤
│           HAL Interfaces (I*Driver, I*Controller)           │  Abstractions
├─────────────────────────────────────────────────────────────┤
│              Drivers (Teensy, ESP32, etc.)                  │  Implementations
├─────────────────────────────────────────────────────────────┤
│                   UI (IView, Widgets)                       │  Presentation (opt.)
├─────────────────────────────────────────────────────────────┤
│                   Config (constexpr)                        │  Configuration
└─────────────────────────────────────────────────────────────┘
```

---

## Namespaces

### Structure

```cpp
namespace oc {
    namespace hal { }       // Hardware Abstraction Layer interfaces
    namespace core { }      // Business logic (EventBus, InputBinding)
    namespace context { }   // Context system (IContext, ContextManager)
    namespace api { }       // ControlAPI (si séparé)
    namespace app { }       // OpenControlApp, AppBuilder
    namespace ui { }        // UI abstractions (IView, optionnel)
    namespace util { }      // Utilitaires (Log, etc.)
}

namespace oc::drivers {
    namespace teensy { }    // Implémentations Teensy
    namespace esp32 { }     // Futur: ESP32
}
```

### Usage

```cpp
// Dans les headers: toujours qualifié
oc::hal::IDisplayDriver
oc::core::event::EventBus
oc::context::IContext

// Dans les .cpp: using déclaration locale OK
using namespace oc::core::event;  // Dans le .cpp seulement
```

---

## Layers

### 1. Entry Point (`main.cpp`)

Minimal. Instancie l'app et délègue tout.

```cpp
#include <oc/app/OpenControlApp.hpp>

oc::app::OpenControlApp app;

void setup() { 
    app = buildApp();  // Via AppBuilder
    app.begin(); 
}

void loop() { 
    app.update(); 
}
```

### 2. Composition Root (`oc::app::OpenControlApp`)

**Responsabilité** : Assembler toutes les dépendances.

- Reçoit les drivers via AppBuilder
- Instancie EventBus, InputBinding, ControlAPI
- Gère le ContextManager
- Point unique de configuration

```cpp
class OpenControlApp {
    // Hardware (owned via unique_ptr)
    std::unique_ptr<hal::IDisplayDriver> display_;
    std::unique_ptr<hal::IMidiTransport> midi_;
    std::unique_ptr<hal::IEncoderController> encoders_;
    std::unique_ptr<hal::IButtonController> buttons_;
    
    // Core services
    core::event::EventBus eventBus_;
    std::unique_ptr<core::input::InputBinding> inputBinding_;
    
    // API and contexts
    std::unique_ptr<ControlAPI> api_;
    std::unique_ptr<context::ContextManager> contexts_;
};
```

### 3. HAL Interfaces (`oc::hal::`)

**Responsabilité** : Abstractions hardware.

| Interface | Rôle |
|-----------|------|
| `IDisplayDriver` | Abstraction écran |
| `IMidiTransport` | MIDI I/O |
| `IEncoderController` | Gestion encodeurs |
| `IButtonController` | Gestion boutons |
| `IMultiplexer` | Multiplexeur (optionnel) |

### 4. Core (`oc::core::`)

**Responsabilité** : Logique métier indépendante du hardware.

| Composant | Rôle |
|-----------|------|
| `EventBus` | Pub/Sub découplé |
| `InputBinding` | Mapping input → action (gestures) |
| `Types` | Enums, structs, aliases partagés |

### 5. Drivers (`oc::drivers::teensy::`)

**Responsabilité** : Implémentations hardware spécifiques.

| Driver | Hardware |
|--------|----------|
| `Ili9341Driver` | Display SPI |
| `TeensyEncoderController` | Encodeurs via EncoderTool |
| `TeensyButtonController` | Boutons (MCU direct ou mux) |
| `CD74HC4067` | Multiplexeur 16 canaux |
| `TeensyUsbMidi` | USB MIDI natif Teensy |

### 6. Context System (`oc::context::`)

**Responsabilité** : Gestion des modes d'utilisation.

```cpp
class IContext {
    virtual bool initialize(ControlAPI& api) = 0;
    virtual void update() = 0;
    virtual void cleanup() = 0;
    virtual const char* getName() const = 0;
    virtual const char* getId() const = 0;
    virtual bool isConnected() const { return true; }
    virtual void onConnected() {}
    virtual void onDisconnected() {}
};
```

### 7. UI (`oc::ui::`) - Optionnel

**Responsabilité** : Abstractions UI (LVGL optionnel).

| Composant | Rôle |
|-----------|------|
| `IView` | Interface vue abstraite |
| `LVGLAdapter` | Helpers pour scoped bindings LVGL |

---

## Patterns

### Event Bus (Pub/Sub)

Communication découplée via événements typés.

```cpp
// Publishing
eventBus_.emit(EncoderChangedEvent(encoderId, value));

// Subscribing
subscription_id_ = eventBus_.on(
    EventCategory::USER_INPUT,
    InputEvent::ENCODER_CHANGED,
    [this](const Event& e) { handleEncoder(e); }
);

// Unsubscribing (in destructor)
eventBus_.off(subscription_id_);
```

**Catégories d'événements** :
- `EventCategory::USER_INPUT` — Boutons, encodeurs
- `EventCategory::MIDI` — CC, Note, SysEx
- `EventCategory::SYSTEM` — Boot, changements mode

### Dependency Injection

Toutes les dépendances injectées via constructeur, jamais de singletons.

```cpp
// ✅ Good: injection explicite
EncoderController(
    const std::vector<EncoderDef>& config,
    IEventBus& eventBus
);

// ❌ Bad: singleton caché
EncoderController() {
    eventBus_ = EventBus::getInstance();  // Anti-pattern
}
```

### Adapter Pattern

Abstraction hardware via interfaces.

```cpp
// Interface
class IMidiTransport {
public:
    virtual void sendCC(uint8_t ch, uint8_t cc, uint8_t val) = 0;
    virtual void sendNoteOn(uint8_t ch, uint8_t note, uint8_t vel) = 0;
    // ...
};

// Teensy implementation
class TeensyUsbMidi : public IMidiTransport {
    void sendCC(uint8_t ch, uint8_t cc, uint8_t val) override {
        usbMIDI.sendControlChange(cc, val, ch + 1);
    }
};
```

### Scoped Bindings

Bindings contextuels avec priorité.

```cpp
// Binding actif seulement si scope visible
api.onTurned(EncoderID::NAV, [this](float v) {
    scrollList(v);
}, visibilityPredicate, scopeId);

// Binding global (toujours actif)
api.onPressed(ButtonID::BACK, [this]() {
    goBack();
});
```

**Priorité** : Scoped > Global (propagation s'arrête si scoped triggered)

### Builder Pattern

Construction fluide de l'application.

```cpp
auto app = oc::app::AppBuilder()
    .display(std::make_unique<Ili9341Driver>(config))
    .midi(std::make_unique<TeensyUsbMidi>())
    .encoders(std::make_unique<TeensyEncoderController<10>>(encoders))
    .buttons(std::make_unique<TeensyButtonController<14>>(buttons, mux))
    .build();
```

---

## Data Flow

### Input → Action

```
Hardware Event (ISR)
       ↓
EncoderController / ButtonController
       ↓
EventBus.emit(InputEvent)
       ↓
   ┌───┴───┐
   ↓       ↓
MidiMapper  InputBinding
   ↓           ↓
MIDI Out    Context callbacks
```

### MIDI In → Context

```
USB MIDI (ISR buffer)
       ↓
IMidiTransport.update()
       ↓
EventBus.emit(MidiEvent)
       ↓
Context.onCC() / onSysEx()
```

---

## Compile-Time Configuration

Toutes les constantes hardware sont `constexpr` pour zero runtime overhead.

```cpp
// Dans le CONSOMMATEUR (pas le framework)
namespace MyProject::Config {

constexpr oc::drivers::teensy::Ili9341Config DISPLAY = {
    .width = 320,
    .height = 240,
    .cs_pin = 28,
    .dc_pin = 0,
    // ...
};

constexpr std::array ENCODERS = {
    oc::drivers::teensy::EncoderDef{1, 22, 23},
    oc::drivers::teensy::EncoderDef{2, 18, 19},
};

} // namespace MyProject::Config
```

---

## Context System

### IContext Interface

```cpp
class IContext {
public:
    virtual ~IContext() = default;

    virtual bool initialize(ControlAPI& api) = 0;
    virtual void update() = 0;
    virtual void cleanup() = 0;

    virtual const char* getName() const = 0;
    virtual const char* getId() const = 0;
    
    // Pour intégrations DAW
    virtual bool isConnected() const { return true; }
    virtual void onConnected() {}
    virtual void onDisconnected() {}
};
```

### ControlAPI

Interface unifiée pour les contexts :

- **Input Binding** : `onPressed()`, `onTurned()`, gestures...
- **MIDI** : `sendCC()`, `sendSysEx()`, `onCC()`...
- **Encoder Control** : `setEncoderPosition()`, `setEncoderMode()`...

### Registration

```cpp
// Framework
app.registerContext<StandaloneContext>("standalone");
app.registerContext<BitwigContext>("bitwig");

// Switch
app.contexts().switchTo("bitwig");
app.contexts().switchToDefault();  // → standalone
```

### loadResources() SFINAE

```cpp
// Si le context a une méthode statique loadResources(), elle est appelée
class BitwigContext : public IContext {
public:
    static void loadResources() {
        // Charger fonts, assets spécifiques
    }
    // ...
};

// ContextManager détecte automatiquement via SFINAE
template<typename T>
void registerContext(const std::string& id) {
    if constexpr (has_load_resources<T>::value) {
        T::loadResources();
    }
    // ...
}
```

---

## Dependency Rules

```
                    ┌──────────────┐
                    │   main.cpp   │
                    └──────┬───────┘
                           │
                    ┌──────▼───────┐
                    │OpenControlApp│
                    └──────┬───────┘
                           │
         ┌─────────────────┼─────────────────┐
         │                 │                 │
    ┌────▼────┐      ┌─────▼─────┐     ┌─────▼─────┐
    │ Context │      │   Core    │     │  Drivers  │
    │ Manager │      │ EventBus  │     │ (Teensy)  │
    └────┬────┘      └─────┬─────┘     └─────┬─────┘
         │                 │                 │
         │           ┌─────▼─────┐           │
         └──────────►│    HAL    │◄──────────┘
                     │ Interfaces│
                     └─────┬─────┘
                           │
                     ┌─────▼─────┐
                     │ Contexts  │
                     └───────────┘
```

**Règle clé** : Les dépendances pointent vers le centre (HAL, Core).
Les couches externes ne se connaissent pas directement.

---

# CODE STYLE

## Naming

### General Rules

| Element | Convention | Example |
|---------|------------|---------|
| Classes | `PascalCase` | `OpenControlApp`, `EncoderController` |
| Interfaces | `I` + `PascalCase` | `IEventBus`, `IView`, `IContext` |
| Structs (data) | `PascalCase` | `ButtonBinding`, `GpioPin` |
| Enums | `PascalCase` | `ButtonBindingType`, `EncoderMode` |
| Enum values | `SCREAMING_SNAKE_CASE` | `LONG_PRESS`, `TURN_WHILE_PRESSED` |
| Functions/Methods | `camelCase` | `onPressed()`, `getSubscriberCount()` |
| Private members | `snake_case_` (trailing `_`) | `event_bus_`, `boot_complete_` |
| Struct members (public) | `camelCase` | `buttonId`, `longPressMs`, `scopeId` |
| Local variables | `snake_case` | `normalized_value`, `press_time` |
| Constants | `SCREAMING_SNAKE_CASE` | `MAX_ACTIVE_NOTES`, `REFRESH_RATE_HZ` |
| Namespaces | `PascalCase` ou `snake_case` | `oc::core::event`, `BaseTheme::Color` |
| Type aliases | `PascalCase` | `EventCallback`, `VisibilityPredicate` |
| Type aliases (IDs) | `PascalCase` with caps `ID` | `ButtonID`, `EncoderID`, `ScopeID`, `SubscriptionID` |
| Template params | `T` or `PascalCase` | `template<typename Callback>` |

### Files

| Type | Convention | Example |
|------|------------|---------|
| Headers | `PascalCase.hpp` | `EventBus.hpp`, `IDisplayDriver.hpp` |
| Sources | `PascalCase.cpp` | `InputBinding.cpp` |
| One file = one concept | Un classe/struct principal par fichier |

---

## Formatting

### Indentation

- **4 spaces** (pas de tabs)

```cpp
void MyClass::doSomething() {
    if (condition) {
        processData();
    }
}
```

### Braces

```cpp
// K&R style (brace on same line)
class MyClass {
public:
    void method();
};

if (condition) {
    // ...
} else {
    // ...
}

for (const auto& item : items) {
    process(item);
}
```

### Line Length

- **Maximum ~100 characters**

```cpp
// Parameters on multiple lines
explicit EncoderController(
    const std::vector<EncoderDef>& encoderSetups,
    IEventBus& eventBus);

// Initializer lists
OpenControlApp::OpenControlApp()
    : display_(),
      eventBus_(),
      inputBinding_() {}
```

---

## Includes

### Strict Order

Groupes séparés par ligne vide :

```cpp
#include "MyClass.hpp"           // 1. Paired header

#include <cstdint>               // 2. C standard headers
#include <cstring>

#include <vector>                // 3. C++ STL headers
#include <functional>
#include <optional>

#include <lvgl.h>                // 4. External libraries
#include <Arduino.h>

#include <oc/core/event/Event.hpp>  // 5. Framework headers
#include <oc/hal/Types.hpp>
```

### Guards

Toujours `#pragma once` :

```cpp
#pragma once

// Header content...
```

---

## Classes

### Declaration Structure

```cpp
class MyClass {
public:
    // 1. Types and aliases
    using Callback = std::function<void()>;

    // 2. Constructors / Destructor
    explicit MyClass(IEventBus& eventBus);
    ~MyClass();

    // 3. Rule of 5 (copy/move)
    MyClass(const MyClass&) = delete;
    MyClass& operator=(const MyClass&) = delete;
    MyClass(MyClass&&) = default;
    MyClass& operator=(MyClass&&) = default;

    // 4. Public methods
    void initialize();
    void update();
    size_t getCount() const;

private:
    // 5. Private methods
    void processInternal();

    // 6. Members (trailing underscore)
    IEventBus& event_bus_;
    bool initialized_ = false;
    std::vector<Item> items_;
};
```

### Constructors

```cpp
// Always explicit for single argument
explicit MyClass(int value);

// Prefer initializer list
MyClass::MyClass(IEventBus& bus, int count)
    : event_bus_(bus),
      count_(count),
      initialized_(false) {}
```

### Const Correctness

```cpp
// Const reference for input parameters
void process(const Event& event);

// Const methods when not mutating
size_t getCount() const;
bool isValid() const;

// Constexpr for compile-time constants
static constexpr uint16_t BUFFER_SIZE = 1024;
```

---

## Enums

### Prefer `enum class`

```cpp
// ✅ Good: strongly-typed
enum class ButtonBindingType : uint8_t {
    PRESS,
    RELEASE,
    LONG_PRESS,
    DOUBLE_TAP,
    COMBO
};

// Usage requires explicit scope
ButtonBindingType type = ButtonBindingType::PRESS;

// ❌ Bad: classic enum
enum ButtonType { PRESS, RELEASE };
```

---

## Documentation

### Language

**All comments MUST be in English** — no exceptions.

### Doxygen Comments (Classes, Interfaces, Methods, Structs, Enums)

Use multiline Doxygen format for all public API:

```cpp
/**
 * @brief Interface for display hardware abstraction
 * 
 * Provides a unified API for display drivers regardless of the
 * underlying hardware (ILI9341, ST7789, etc.).
 * 
 * @note Implementations are platform-specific (see drivers/teensy/)
 */
class IDisplayDriver {
public:
    virtual ~IDisplayDriver() = default;
    
    /**
     * @brief Initialize display hardware
     * 
     * Must be called before any other operations. Configures SPI,
     * sets up framebuffers, and prepares the display for rendering.
     */
    virtual void init() = 0;
    
    /**
     * @brief Flush pixel buffer to display
     * 
     * @param buffer Pointer to pixel data (RGB565 format)
     * @param area   Rectangle defining the update region
     * 
     * @note For async implementations, call flush callback when done
     */
    virtual void flush(const void* buffer, const Rect& area) = 0;
};

/**
 * @brief Button binding types for input handling
 */
enum class ButtonBindingType : uint8_t {
    PRESS,           ///< Triggered on button press
    RELEASE,         ///< Triggered on button release
    LONG_PRESS,      ///< Triggered after hold duration
    DOUBLE_TAP,      ///< Triggered on rapid double press
    COMBO            ///< Triggered when two buttons pressed together
};

/**
 * @brief Configuration for ILI9341 display driver
 */
struct Ili9341Config {
    uint16_t width;      ///< Screen width in pixels
    uint16_t height;     ///< Screen height in pixels
    uint8_t cs_pin;      ///< SPI chip select pin
    uint8_t dc_pin;      ///< Data/command pin
    uint8_t rst_pin;     ///< Reset pin
    uint32_t spi_speed;  ///< SPI clock frequency in Hz
};
```

### Inline Comments

**Use sparingly** — only when necessary to explain:
- **WHY** something is done (not what)
- **HOW** a non-obvious algorithm works

```cpp
// ❌ Bad: explains "what" (obvious from code)
// Increment counter
count++;

// ❌ Bad: unnecessary comment
// Check if valid
if (isValid()) { ... }

// ✅ Good: explains "why"
// Skip first frame to allow LVGL layout calculation
if (frame_count_++ == 0) return;

// ✅ Good: explains non-obvious behavior
// Teensy USB MIDI uses 1-based channels, convert from 0-based
usbMIDI.sendControlChange(cc, value, channel + 1);

// ✅ Good: documents a workaround or constraint
// CD74HC4067 needs 20µs settling time after channel switch
delayMicroseconds(20);
```

### Code Sections

Use section headers for logical groupings in large files:

```cpp
// ═══════════════════════════════════════════════════
// INPUT BINDING - Global (always active)
// ═══════════════════════════════════════════════════

void onPressed(...);
void onReleased(...);

// ═══════════════════════════════════════════════════
// INPUT BINDING - Scoped (active when scope visible)
// ═══════════════════════════════════════════════════

void onPressed(..., VisibilityPredicate, ScopeId);

// ═══════════════════════════════════════════════════
// MIDI OUTPUT
// ═══════════════════════════════════════════════════

void sendCC(...);
void sendNoteOn(...);
```

### What NOT to Comment

```cpp
// ❌ Don't comment obvious code
int count = 0;  // Initialize count to zero

// ❌ Don't comment self-documenting names
void processEncoderEvent(const EncoderEvent& event);  // Process encoder event

// ❌ Don't leave TODO/FIXME in committed code (use issue tracker)
// TODO: fix this later  ❌
```

---

## Embedded Specifics

### Memory

```cpp
// Prefer std::array for fixed sizes
std::array<uint8_t, 16> buffer;  // ✅
std::vector<uint8_t> buffer(16); // ❌ if size known

// Constexpr for constants
static constexpr size_t MAX_ITEMS = 32;

// Avoid allocations in hot paths
void update() {
    // ❌ std::string temp = "...";
    // ✅ const char* temp = "...";
}
```

### RAII

```cpp
// Automatic cleanup in destructors
~MyWidget() {
    if (timer_) {
        lv_timer_delete(timer_);
        timer_ = nullptr;
    }
}
```

### Smart Pointers

```cpp
// unique_ptr for exclusive ownership
std::unique_ptr<hal::IDisplayDriver> display_;

// References for injected dependencies (no ownership)
IEventBus& event_bus_;  // ✅
std::shared_ptr<IEventBus> event_bus_;  // ❌ unnecessary overhead
```

### Conditional Logging

```cpp
// Use macros
LOGLN("[Module] Initialized");
LOGF("[Module] Value: %d\n", value);

// Compiled to no-op without DEBUG_LOGS
#ifdef DEBUG_LOGS
    // Debug-only code
#endif
```

---

## Common Patterns

### Event Subscription

```cpp
// In constructor
MyClass::MyClass(IEventBus& bus) : event_bus_(bus) {
    subscription_id_ = event_bus_.on(
        EventCategory::USER_INPUT,
        InputEvent::BUTTON_PRESS,
        [this](const Event& e) { onButtonPress(e); }
    );
}

// In destructor
MyClass::~MyClass() {
    event_bus_.off(subscription_id_);
}
```

### Early Return

```cpp
// Prefer early return for readability
void process(const Data& data) {
    if (!data.isValid()) return;
    if (data.isEmpty()) return;

    // Main logic
    doWork(data);
}
```

### Designated Initializers (C++20)

```cpp
ButtonBinding binding{
    .type = ButtonBindingType::PRESS,
    .buttonId = buttonId,
    .action = std::move(callback),
    .enabled = true
};
```

---

## Anti-patterns to Avoid

### ❌ Don't Do

```cpp
using namespace std;              // Namespace pollution
#define MAX_SIZE 100             // Use constexpr
void foo(int x) { }              // Missing explicit if constructor
class foo { };                   // Wrong naming (lowercase)
int m_member;                    // m_ prefix (use _ suffix)
int member;                      // No suffix for private member
new Object();                    // Prefer smart pointers/RAII
```

### ✅ Do

```cpp
constexpr size_t MAX_SIZE = 100;
explicit Foo(int x);
class Foo { };
int member_;
std::make_unique<Object>();
```

---

## Tools

### .clang-format

```yaml
BasedOnStyle: Google
Standard: c++20
IndentWidth: 4
ColumnLimit: 100

AllowShortBlocksOnASingleLine: Always
AllowShortFunctionsOnASingleLine: All
AllowShortIfStatementsOnASingleLine: AllIfsAndElse

IncludeBlocks: Regroup
IncludeCategories:
  - Regex: '^"[^/]*\.hpp"'
    Priority: 1
  - Regex: '^<c(stdint|string)>'
    Priority: 2
  - Regex: '^<(vector|map|functional)>'
    Priority: 3
  - Regex: '^<'
    Priority: 4
  - Regex: '^"'
    Priority: 5
```

### .clangd

```yaml
CompileFlags:
  CompilationDatabase: .

Diagnostics:
  Suppress:
    - ovl_diff_return_type
```

### VS Code Extensions

```json
{
    "recommendations": [
        "platformio.platformio-ide",
        "llvm-vs-code-extensions.vscode-clangd"
    ],
    "unwantedRecommendations": [
        "ms-vscode.cpptools-extension-pack"
    ]
}
```

---

## Performance Guidelines

| Technique | Effet |
|-----------|-------|
| `constexpr` config | Zero runtime cost |
| `std::array` over `std::vector` | No heap allocation |
| Diff buffer display | Reduces SPI transfers |
| Batch event flush | Prevents ISR flood |
| `std::optional` | Lazy initialization |
| RAII | Automatic cleanup |

### Rules

- Éviter allocations dynamiques dans `update()`
- Préférer `std::array` pour tailles connues
- Utiliser `constexpr` pour toutes les constantes
- RAII systématique (cleanup dans destructeurs)

---

## Versioning

### Semantic Versioning

**Framework Version** (`MAJOR.MINOR.PATCH[-PRERELEASE]`)
- `MAJOR` : Breaking changes
- `MINOR` : Nouvelles features (backward-compatible)
- `PATCH` : Bug fixes

**API Version** (`MAJOR.MINOR.PATCH`)
- Évolue indépendamment
- `MAJOR` : Breaking API changes (contexts doivent s'adapter)

### Version.hpp

```cpp
namespace oc {
    constexpr uint8_t VERSION_MAJOR = 0;
    constexpr uint8_t VERSION_MINOR = 1;
    constexpr uint8_t VERSION_PATCH = 0;
    constexpr const char* VERSION = "0.1.0";
}

namespace oc::api {
    constexpr uint8_t VERSION_MAJOR = 1;
    constexpr uint8_t VERSION_MINOR = 0;
    constexpr uint8_t VERSION_PATCH = 0;
}
```
