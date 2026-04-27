# Open Control Framework Architecture

This document exists to make the framework structure predictable and reproducible.

## Non-Negotiable Rules

1) Folder = namespace

- Every C++ file under `src/oc/**` must declare `namespace` that matches its folder path.
- Example: `src/oc/core/input/InputBinding.hpp` must declare `namespace oc::core::input { ... }`.

2) One-way dependencies

- Dependencies must flow in a single direction (no cycles).
- Each module may only include headers from approved lower modules.

3) No `using namespace` in headers

- `using namespace` directives are forbidden in `*.hpp`.
- If you want aliases, re-export explicitly with `constexpr` values or `using X = ...`.

## Modules And Responsibilities

The `src/oc/` tree is split into modules by the first directory level:

- `src/oc/type/` (Level 0): foundational types (Result, IDs, callbacks, events)
- `src/oc/log/`, `src/oc/time/`, `src/oc/util/`, `src/oc/codec/`, `src/oc/debug/`, `src/oc/realtime/` (Level 0): platform-agnostic utilities
- `src/oc/interface/` (Level 1): pure abstractions (HAL + service interfaces)
- `src/oc/core/`, `src/oc/state/`, `src/oc/impl/` (Level 2): framework internals
- `src/oc/api/`, `src/oc/context/` (Level 3): user-facing facade + context orchestration
- `src/oc/app/` (Level 4): application composition (builder + main loop)

## "If I Create A New X, Where Does It Go?"

- New ID / error type / basic POD used everywhere: `src/oc/type/`
- New realtime primitive with no framework ownership: `src/oc/realtime/`
- New HAL abstraction (driver contract): `src/oc/interface/`
- New mock/null implementation of an interface: `src/oc/impl/`
- New internal algorithm (input logic, routing, etc.): `src/oc/core/`
- New reactive primitive / persistence mechanism: `src/oc/state/`
- New user-facing convenience wrapper: `src/oc/api/`
- New context lifecycle feature: `src/oc/context/`
- New top-level composition / wiring: `src/oc/app/`

## Contexts And API Injection

- `oc::interface::IContext` is the pure lifecycle interface (no higher-level deps).
- Contexts that need Button/Encoder/Midi APIs should inherit from `oc::context::ContextBase`.
- API injection is performed by `oc::context::ContextManager` through `oc::context::IContextWithAPIs`.

## Guardrails

Run the architecture checks locally:

```bash
python3 script/dev/check_architecture.py
```

This enforces:
- namespace/path coherence
- forbidden module dependencies
- no `using namespace` in headers

## Include Rules (Module Dependency Matrix)

Includes are constrained by module boundaries. The module is the first folder
under `src/oc/`.

Allowed high-level dependencies:

- `type`: must not include any `oc/...` header
- `interface`: may include `type`
- `impl`: may include `interface`, `type`
- `core`: may include `interface`, `type`, `oc/Config.hpp`, and Level-0 utilities (`log`, `time`, `util`, `codec`, `debug`, `realtime`)
- `state`: may include `interface`, `type`, `oc/Config.hpp`, and Level-0 utilities (`log`, `time`, `util`, `codec`, `debug`, `realtime`)
- `api`: may include `core`, `interface`, `type` (+ Level-0 utilities)
- `context`: may include `api`, `core`, `state`, `interface`, `type` (+ Level-0 utilities)
- `app`: may include everything (composition root)

The enforced source of truth is `script/dev/check_architecture.py`.
