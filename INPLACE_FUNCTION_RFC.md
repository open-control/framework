# Inplace Callback RFC

## Goal

Reduce hidden heap allocations and improve determinism in the framework callback layer
without materially degrading API ergonomics for application code.

This RFC does **not** aim to make every part of the framework allocation-free.
It aims to make the most central callback paths:

- predictable in memory behavior
- cheaper in runtime overhead
- easier to reason about on embedded targets

while preserving a friendly lambda-based DX.

## Current Problem

The framework now has a stronger deterministic core than before:

- `Signal<T, N>` is fixed-capacity
- `NotificationQueue` is fixed-capacity
- `StaticSignalWatcher` exists for critical paths

But many core callback types still rely on `std::function`, which introduces:

- possible heap allocations depending on capture size and STL implementation
- less predictable code size and runtime behavior
- higher memory overhead than raw function pointers or inline delegates
- hidden costs in hot or frequently-instantiated paths

This is especially relevant in:

- `Signal` subscribers
- generic callback type aliases
- MIDI input callback registration
- context factories
- visibility / binding / persistence helpers

## Design Principle

The framework should support **two levels of ergonomics**:

1. A default callback primitive that remains lambda-friendly and easy to use.
2. A bounded inline storage policy so small captures stay allocation-free.

The goal is not to replace every convenience API with manual function pointers.
The goal is to replace `std::function` with a deterministic-enough inline callback
wrapper in the places that matter most.

## Proposal

Introduce a new callback wrapper in the framework:

- `oc::type::InplaceFunction<Signature, Capacity, Alignment = alignof(std::max_align_t)>`

Example:

```cpp
using ActionCallback = InplaceFunction<void(), 16>;
using EncoderCallback = InplaceFunction<void(EncoderID, float), 16>;
using MidiClockCallback = InplaceFunction<void(uint64_t), 16>;
```

### Desired Properties

- stores small callables inline
- no heap allocation for captures that fit in `Capacity`
- move-only is acceptable
- copyability is optional, but move support is required
- explicit `operator bool()`
- same invocation style as `std::function`
- clear failure behavior if callable exceeds capacity

### Failure Policy

The framework should fail loudly in debug builds when a callable does not fit:

- log a precise error with requested size, capacity, and callback label if available
- assert immediately

No silent fallback to heap allocation is recommended for this primitive.

That keeps the contract explicit:

- if it compiles and runs, callback storage is bounded
- if a capture is too large, the failure is immediate and actionable

## Why Not Replace With Raw Function Pointers Everywhere

That would improve determinism but significantly hurt DX:

- member captures become awkward
- local lambdas become much harder to use
- common patterns in views, handlers, and context wiring become noisy

`InplaceFunction` gives most of the determinism benefit while preserving the
shape of the current APIs.

## Proposed API

New file:

- `src/oc/type/InplaceFunction.hpp`

Suggested surface:

```cpp
namespace oc::type {

template <typename Signature, size_t Capacity, size_t Alignment = alignof(std::max_align_t)>
class InplaceFunction;

template <typename R, typename... Args, size_t Capacity, size_t Alignment>
class InplaceFunction<R(Args...), Capacity, Alignment> {
public:
    InplaceFunction() = default;
    InplaceFunction(std::nullptr_t) {}

    template <typename F>
    InplaceFunction(F&& fn);

    InplaceFunction(InplaceFunction&& other) noexcept;
    InplaceFunction& operator=(InplaceFunction&& other) noexcept;

    InplaceFunction(const InplaceFunction&) = delete;
    InplaceFunction& operator=(const InplaceFunction&) = delete;

    ~InplaceFunction();

    explicit operator bool() const noexcept;
    R operator()(Args... args) const;
    void reset() noexcept;

    static constexpr size_t capacity();
};

}  // namespace oc::type
```

### Internal Shape

Recommended implementation:

- inline storage buffer:
  - `std::byte storage_[Capacity]`
- vtable-like function pointers:
  - invoke
  - move
  - destroy

This keeps code compact and avoids RTTI or dynamic allocation.

## Capacity Strategy

Do **not** use one universal capacity everywhere.

Use small, role-based capacities instead.

Recommended defaults:

- `ActionCallback`: 16 bytes
- `EncoderActionCallback`: 16 bytes
- `ButtonCallback`: 16 bytes
- `EncoderCallback`: 16 bytes
- `IsActiveFn`: 16 bytes
- MIDI callbacks: 16 or 24 bytes depending on common capture patterns
- `ContextFactory`: 24 or 32 bytes
- signal subscriber callback:
  - leave configurable per `Signal` alias if needed later
  - start with 16 bytes

Rationale:

- most view/presenter lambdas capture only `this`
- `this` + one small scalar usually fits in 16 bytes on 32/64-bit targets
- context factories often capture one or two references, so 24-32 bytes is safer

## Primary Migration Targets

### Phase 1: Generic callback aliases

Files:

- `src/oc/type/Callbacks.hpp`
- `src/oc/interface/IMidi.hpp`

Replace:

- `std::function<void()>`
- `std::function<void(float)>`
- `std::function<void(ButtonID, ButtonEvent)>`
- `std::function<void(EncoderID, float)>`
- MIDI callback aliases

with `InplaceFunction<...>`.

Why first:

- central, low-friction, high value
- immediate determinism improvement across the framework
- minimal API churn for users

### Phase 2: `Signal`

File:

- `src/oc/state/Signal.hpp`

Replace `Signal::Callback = std::function<void(const T&)>` with an inline callback alias.

Important note:

- this is high impact because every `Signal<T, N>` instantiation embeds `N` callbacks
- capacity must be kept intentionally small

Expected benefit:

- fewer hidden allocations at subscription time
- better memory predictability in subscriber slots

Expected cost:

- possible RAM increase per signal instance depending on capacity choice
- must be measured carefully on `core`

### Phase 3: `SignalWatcher` dynamic path

File:

- `src/oc/state/SignalWatcher.hpp`

Replace:

- `std::function<void()> callback_`

with a bounded inline callback type.

This complements the existing `StaticSignalWatcher` without changing the public
usage pattern of the dynamic watcher.

### Phase 4: `ContextManager`

File:

- `src/oc/context/ContextManager.hpp`

Replace:

- `ContextFactory = std::function<std::unique_ptr<IContext>()>`

with:

- `InplaceFunction<std::unique_ptr<IContext>(), 24>` or `32`

This is boot-time code, so it is less urgent than `Signal`, but still valuable.

### Phase 5: utility helpers

Files:

- `src/oc/state/ExclusiveVisibilityStack.hpp`
- `src/oc/state/AutoPersist.hpp`
- `src/oc/state/AutoPersistIncremental.hpp`

These are secondary targets once the core callback primitive is stable.

## Areas That Should Not Be Prioritized First

### `OpenControlApp` ownership model

File:

- `src/oc/app/OpenControlApp.hpp`

The heavy use of `std::unique_ptr` here is not the best first target.
It affects construction/lifetime semantics more than callback determinism.

### `BindingRegistry`

File:

- `src/oc/core/input/BindingRegistry.hpp`

It still uses `std::vector`, but it already reserves upfront and is less risky
than callback allocation paths.

### `SignalVector::toVector`

File:

- `src/oc/state/SignalVector.hpp`

Worth documenting and perhaps tightening later, but it is not a first-order
determinism problem compared to callback storage.

## DX Impact

Expected user-facing impact should stay small if the migration is done carefully.

### What should remain unchanged

- lambda syntax at call sites
- callback registration APIs
- watcher and signal usage patterns
- MIDI callback registration shape

### What will become more explicit

- oversized captures will fail loudly instead of sometimes allocating
- a few APIs may need carefully chosen capacities
- debugging logs should report when a callable exceeds inline storage

### Main DX risk

A lambda that used to "just work" with `std::function` may overflow the inline
capacity if it captures too much state.

Mitigation:

- start with capacities chosen from real call sites
- add good diagnostics
- document preferred capture style:
  - prefer capturing `this`
  - avoid copying large state into lambdas

## Memory Impact

This must be measured, not guessed.

Expected tradeoff:

- less heap usage and fewer hidden allocations
- more explicit inline storage in objects that embed callbacks

This means:

- some instances may get larger
- total system behavior becomes more predictable

Most sensitive location:

- `Signal<T, N>` because subscriber slots are embedded per signal instance

Therefore:

- migrate callback aliases first
- then measure
- only then migrate `Signal`

## Determinism Impact

Very positive in the most important places:

- fewer heap-dependent callback constructions
- tighter control of callback storage
- failure mode becomes explicit and local

This does **not** make the entire framework perfectly deterministic by itself.
Other dynamic subsystems still exist, especially:

- `EventBus`
- dynamic `SignalWatcher`
- context ownership and some helper containers

Still, it is one of the best next steps because it improves determinism across
many APIs without forcing a harsh DX downgrade.

## Testing Plan

### New tests for `InplaceFunction`

Add:

- empty function behavior
- invocation with lambda capturing `this`
- invocation with move-only capture if supported
- move construction / move assignment
- reset semantics
- overflow diagnostic path

### Regression tests

Validate existing suites still pass:

- signal tests
- signal watcher tests
- notification queue tests
- event bus tests
- MIDI-facing integration where available

### Consumer verification

At minimum:

- `open-control/framework`: `pio test -e native`
- `midi-studio/core`: `pio run -e dev`
- `midi-studio/plugin-bitwig`: `pio run`

## Rollout Plan

### Step 0

Implement `InplaceFunction` in isolation with dedicated unit tests.

### Step 1

Migrate `Callbacks.hpp` and `IMidi.hpp`.

This gives a broad benefit with low structural risk.

### Step 2

Migrate `SignalWatcher` dynamic callback storage.

### Step 3

Measure size and behavior on consumers.

Especially:

- `RAM1 variables`
- `RAM1 code`
- boot behavior on `core`

### Step 4

Migrate `Signal.hpp` if measurements are acceptable.

This step should be gated by real memory numbers, not intuition.

### Step 5

Migrate `ContextManager` factory callback.

### Step 6

Optionally expand to helper utilities.

## Recommendation

The next implementation step should be:

1. add `InplaceFunction`
2. wire it into `Callbacks.hpp` and `IMidi.hpp`
3. measure

Only after that should `Signal.hpp` be migrated.

That gives the best balance of:

- real determinism gains
- bounded complexity
- low DX disruption
- low risk of another large RAM regression

## Non-Goal

This RFC does not propose replacing every callback or every container in the
framework immediately.

It is a targeted plan for callback determinism where it matters most.
