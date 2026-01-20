# Migration Plan v2 - OpenControl Framework

## Règle Fondamentale : Cohérence Namespace/Dossier

**CRITIQUE : Les namespaces C++ DOIVENT TOUJOURS refléter exactement la structure des dossiers.**

```
Dossier                          → Namespace
─────────────────────────────────────────────────
src/oc/                          → oc
src/oc/interface/                → oc::interface
src/oc/impl/                     → oc::impl
src/oc/core/                     → oc::core
src/oc/core/event/               → oc::core::event
src/oc/core/input/               → oc::core::input
src/oc/context/                  → oc::context
src/oc/api/                      → oc::api
src/oc/app/                      → oc::app
src/oc/state/                    → oc::state
```

**Règles de qualification:**
- Dans `oc::api`, référencer `oc::interface::IButton` comme `interface::IButton`
- Dans `oc::core::input`, référencer `oc::interface::IEventBus` comme `interface::IEventBus`
- Les types de base (`ButtonID`, `EncoderID`, `TimeProvider`) restent dans `oc` (fichier `Types.hpp`)
- JAMAIS de `using namespace` pour importer un namespace complet dans un header
- Préférer la qualification explicite pour la clarté

---

## Phase 1 : Framework (COMPLÉTÉE)

### 1.1 Structure des dossiers
```
framework/src/oc/
├── interface/        # Toutes les interfaces (I*.hpp)
│   ├── IButton.hpp         # oc::interface::IButton
│   ├── IEncoder.hpp        # oc::interface::IEncoder + EncoderMode
│   ├── IEncoderHardware.hpp
│   ├── IMidi.hpp           # oc::interface::IMidi
│   ├── ITransport.hpp      # oc::interface::ITransport
│   ├── IStorage.hpp        # oc::interface::IStorage
│   ├── IDisplay.hpp        # oc::interface::IDisplay
│   ├── IGpio.hpp
│   ├── IMultiplexer.hpp
│   ├── IContext.hpp        # oc::interface::IContext
│   ├── IContextSwitcher.hpp # oc::interface::IContextSwitcher + ContextInfo
│   ├── IEventBus.hpp       # oc::interface::IEventBus + SubscriptionID + EventCallback
│   └── Types.hpp           # oc::ButtonID, oc::EncoderID, oc::TimeProvider (EXCEPTION: namespace oc)
├── impl/             # Implémentations null/mock
│   ├── NullMidi.hpp        # oc::impl::NullMidi
│   ├── NullStorage.hpp     # oc::impl::NullStorage
│   └── MemoryStorage.hpp   # oc::impl::MemoryStorage
├── core/
│   ├── event/
│   │   ├── Event.hpp       # oc::core::event::Event, EventCategoryType, EventType
│   │   ├── Events.hpp
│   │   └── EventBus.hpp    # oc::core::event::EventBus : interface::IEventBus
│   └── input/
│       ├── InputBinding.hpp
│       ├── EncoderLogic.hpp
│       └── ...
├── context/
│   ├── APIs.hpp            # oc::context::APIs
│   ├── ContextManager.hpp  # oc::context::ContextManager : interface::IContextSwitcher
│   └── Requirements.hpp
├── api/
│   ├── ButtonAPI.hpp       # oc::api::ButtonAPI
│   ├── EncoderAPI.hpp      # oc::api::EncoderAPI
│   ├── MidiAPI.hpp         # oc::api::MidiAPI
│   └── *Proxy.hpp
├── app/
│   ├── AppBuilder.hpp      # oc::app::AppBuilder
│   └── OpenControlApp.hpp  # oc::app::OpenControlApp
└── state/
    ├── Settings.hpp        # oc::state::Settings<T>
    └── AutoPersist.hpp
```

### 1.2 Mappings effectués
| Ancien                          | Nouveau                           | Namespace              |
|--------------------------------|-----------------------------------|------------------------|
| `hal/IButtonController.hpp`    | `interface/IButton.hpp`           | `oc::interface`        |
| `hal/IEncoderController.hpp`   | `interface/IEncoder.hpp`          | `oc::interface`        |
| `hal/IMidiTransport.hpp`       | `interface/IMidi.hpp`             | `oc::interface`        |
| `hal/IFrameTransport.hpp`      | `interface/ITransport.hpp`        | `oc::interface`        |
| `hal/IStorageBackend.hpp`      | `interface/IStorage.hpp`          | `oc::interface`        |
| `hal/IDisplayDriver.hpp`       | `interface/IDisplay.hpp`          | `oc::interface`        |
| `hal/Types.hpp`                | `interface/Types.hpp`             | `oc` (exception)       |
| `context/IContext.hpp`         | `interface/IContext.hpp`          | `oc::interface`        |
| `context/IContextSwitcher.hpp` | `interface/IContextSwitcher.hpp`  | `oc::interface`        |
| `core/event/IEventBus.hpp`     | `interface/IEventBus.hpp`         | `oc::interface`        |
| `hal/NullMidiTransport.hpp`    | `impl/NullMidi.hpp`               | `oc::impl`             |

### 1.3 Validation
- [x] Compilation : `pio run -e native` OK
- [x] Tests : 252/252 passés

---

## Phase 2 : HALs

**RÈGLE NAMESPACE : Chaque HAL conserve son namespace existant (correspond au chemin). Seuls les includes et qualifications des interfaces changent.**

### 2.1 hal-common

Structure actuelle (INCHANGÉE):
```
hal-common/
└── src/
    └── oc/
        └── hal/
            └── common/
                └── embedded/
                    ├── ButtonDef.hpp       # oc::hal::common::embedded
                    ├── EncoderDef.hpp      # oc::hal::common::embedded
                    ├── GpioPin.hpp         # oc::hal::common::embedded
                    └── Types.hpp           # oc::hal::common::embedded
```

**Namespace:** `oc::hal::common::embedded` (INCHANGÉ - correspond au chemin)

**Changements requis:**
Les fichiers de hal-common ne semblent pas inclure directement les interfaces du framework.
Vérifier si des mises à jour sont nécessaires.

### 2.2 hal-sdl

**Namespace:** `oc::hal::sdl` (à vérifier)

### 2.3 Validation Phase 2
- [ ] Vérifier les includes dans hal-common
- [ ] Compilation de chaque HAL

---

## Phase 3 : hal-teensy

**Namespace:** `oc::hal::teensy` (INCHANGÉ - correspond au chemin)

Structure actuelle:
```
hal-teensy/
└── src/
    └── oc/
        └── hal/
            └── teensy/
                ├── ButtonController.hpp     # oc::hal::teensy::ButtonController
                ├── EncoderController.hpp    # oc::hal::teensy::EncoderController
                ├── UsbMidi.hpp              # oc::hal::teensy::UsbMidi
                ├── UsbSerial.hpp            # oc::hal::teensy::UsbSerial
                ├── TeensyGpio.hpp           # oc::hal::teensy::TeensyGpio
                ├── GenericMux.hpp           # oc::hal::teensy::GenericMux
                ├── Ili9341.hpp              # oc::hal::teensy::Ili9341
                ├── EEPROMBackend.hpp        # oc::hal::teensy::EEPROMBackend
                ├── LittleFSBackend.hpp      # oc::hal::teensy::LittleFSBackend
                ├── SDCardBackend.hpp        # oc::hal::teensy::SDCardBackend
                └── ...
```

**Changements requis dans chaque fichier:**

1. **Includes** - Remplacer:
```cpp
// AVANT
#include <oc/hal/IButtonController.hpp>
#include <oc/hal/IEncoderController.hpp>
#include <oc/hal/IMidiTransport.hpp>
#include <oc/hal/IGpio.hpp>
#include <oc/hal/IMultiplexer.hpp>
#include <oc/hal/IStorageBackend.hpp>
#include <oc/hal/IDisplayDriver.hpp>
#include <oc/hal/Types.hpp>

// APRÈS
#include <oc/interface/IButton.hpp>
#include <oc/interface/IEncoder.hpp>
#include <oc/interface/IMidi.hpp>
#include <oc/interface/IGpio.hpp>
#include <oc/interface/IMultiplexer.hpp>
#include <oc/interface/IStorage.hpp>
#include <oc/interface/IDisplay.hpp>
#include <oc/interface/Types.hpp>
```

2. **Héritage** - Qualifier avec le namespace complet:
```cpp
// AVANT (dans namespace oc::hal::teensy)
class ButtonController : public oc::hal::IButtonController { ... }

// APRÈS (dans namespace oc::hal::teensy)
class ButtonController : public oc::interface::IButton { ... }
```

3. **Références aux types** - Utiliser les bons namespaces:
```cpp
// AVANT
oc::hal::IGpio& gpio_
oc::hal::ButtonID id

// APRÈS
oc::interface::IGpio& gpio_
oc::ButtonID id  // (Types restent dans oc::)
```

4. **Includes hal-common** - Vérifier le chemin:
```cpp
// AVANT (dans hal-teensy)
#include <oc/hal/embedded/ButtonDef.hpp>

// APRÈS (chemin complet vers hal-common)
#include <oc/hal/common/embedded/ButtonDef.hpp>
```

### Validation Phase 3
- [ ] Mettre à jour tous les includes
- [ ] Mettre à jour toutes les qualifications de types
- [ ] Compilation teensy41 (`pio run -e dev`)
- [ ] Tests sur hardware si possible

---

## Phase 4 : ui-lvgl

**Namespace:** `oc::ui::lvgl`

Structure attendue:
```
ui-lvgl/
└── src/
    └── oc/
        └── ui/
            └── lvgl/
                ├── LvglDisplay.hpp    # oc::ui::lvgl::LvglDisplay
                └── ...
```

### Validation Phase 4
- [ ] Compilation avec LVGL
- [ ] Tests visuels si possible

---

## Phase 5 : Examples

Chaque example doit mettre à jour ses includes:

```cpp
// Avant
#include <oc/hal/IButtonController.hpp>
#include <oc/hal/Types.hpp>

// Après
#include <oc/interface/IButton.hpp>
#include <oc/interface/Types.hpp>
```

Examples à mettre à jour:
- [ ] example-teensy41-minimal
- [ ] example-teensy41-01-midi-output
- [ ] example-teensy41-02-encoders
- [ ] example-teensy41-03-buttons
- [ ] example-teensy41-lvgl

### Validation Phase 5
- [ ] Compilation de chaque example

---

## Phase 6 : protocol-codegen

Vérifier si le générateur de code produit des includes ou namespaces à mettre à jour.

**Namespace généré:** Doit correspondre à la structure de sortie

### Validation Phase 6
- [ ] Régénérer le code
- [ ] Vérifier la compilation

---

## Phase 7 : Documentation

- [ ] Mettre à jour les exemples dans les docstrings
- [ ] Vérifier les liens dans les commentaires `@see`
- [ ] Mettre à jour le README si nécessaire

---

## Phase 8 : Git

### 8.1 Préparation
```bash
# Vérifier que tout est propre
git status

# S'assurer que tous les tests passent
pio test -e native
```

### 8.2 Commit
Message de commit suggéré:
```
refactor: reorganize namespaces to match directory structure

- Move all interfaces to oc::interface (IButton, IEncoder, IMidi, etc.)
- Move null implementations to oc::impl
- Types (ButtonID, EncoderID, TimeProvider) remain in oc namespace
- Update all references throughout codebase
- Ensure namespace = directory path pattern

BREAKING CHANGE: All hal:: references must be updated to interface::
```

### 8.3 Vérifications finales
- [ ] `git diff --stat` pour voir l'étendue des changements
- [ ] Pas de fichiers non suivis oubliés
- [ ] Tests passent une dernière fois

---

## Annexe : Patterns de Qualification

### Depuis `oc::api`
```cpp
namespace oc::api {
    class ButtonAPI {
        interface::IButton& hw_;  // oc::interface::IButton
        void setCallback(ButtonCallback cb);  // oc::ButtonCallback (dans oc)
    };
}
```

### Depuis `oc::core::input`
```cpp
namespace oc::core::input {
    class InputBinding {
        interface::IEventBus& bus_;  // oc::interface::IEventBus
        interface::SubscriptionID sub_;  // oc::interface::SubscriptionID
    };
}
```

### Depuis `oc::context`
```cpp
namespace oc::context {
    class ContextManager : public interface::IContextSwitcher {
        // Hérite de oc::interface::IContextSwitcher
    };
}
```

### Depuis `oc::impl`
```cpp
namespace oc::impl {
    class NullStorage : public interface::IStorage {
        // Hérite de oc::interface::IStorage
    };
}
```
