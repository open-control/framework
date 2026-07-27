# Input routing policy

This contract separates physical button gestures from instantaneous encoder
turns. It is intentionally fail-closed: a missing or ambiguous contextual
binding must do nothing instead of triggering an unrelated action.

## Strict physical-button rules

Applications opt in with:

```cpp
InputConfig{
    .releaseRoutingPolicy = ReleaseRoutingPolicy::OwnerOnly,
    .gestureRoutingPolicy = GestureRoutingPolicy::PressScoped,
    .ambiguityPolicy = BindingAmbiguityPolicy::FailClosed,
    .globalRoutingPolicy = GlobalRoutingPolicy::ExplicitPassThroughOnly,
};
```

The resulting contract has eight rules:

1. **Authority is captured on physical press.** The active overlay or view
   scope at press time owns the gesture.
2. **The whole gesture keeps one route.** Authority is fixed before Press is
   dispatched. A synchronous Press callback may establish an inline mode in
   that same authority; at the end of that callback, the remaining phase
   bindings are frozen. Release, long press, double tap, and combo cannot then
   migrate because a later overlay, mode, or predicate changed.
3. **Predicates fail closed.** A delayed phase rechecks only its captured
   binding. If that binding is no longer active, the phase is consumed; the
   dispatcher does not search for another action.
4. **Authority transitions quarantine held gestures.** A real overlay
   show/hide/replace consumes buttons held across that transition. An
   idempotent `show()` does not. This prevents the opening or closing release
   from leaking into the newly authoritative scope. The manager observes the
   visibility transition itself, so this invariant also covers a legacy caller
   that still mutates the underlying visibility stack directly.
5. **Handoffs are explicit.** `handoffPress(button, targetScope)` is the only
   supported way to transfer a held gesture. Use it only when the same physical
   interaction deliberately continues inside the newly opened scope.
6. **Active scopes consume missing bindings.** An unbound button in an
   authoritative scope never falls through to an unrelated global binding.
7. **Ambiguity is an error, not ordering.** If multiple active bindings share
   the highest priority for one button, phase, and scope, the gesture is
   consumed and diagnostics report the ambiguity. Use `.priority()` only when
   alternatives are intentional and their precedence is explicit.
8. **Global pass-through is reserved and declared.** A global binding may cross
   an active view or overlay only when it uses `.globalPassThrough()`. This
   reserves a physical control across the product instead of relying on
   fallback behavior.

## Binding vocabulary

```cpp
buttons.button(BUTTON)
    .release()
    .scope(overlayScope)
    .when([&] { return actionAvailable(); })
    .then([&] { performAction(); });
```

- `.scope(scope)` declares the sole authority domain for the binding.
- `.when(predicate)` expresses availability within that scope. It is not a
  routing fallback.
- `.priority(value)` resolves deliberate alternatives within the same scope and
  gesture phase. Equal top priorities are rejected in strict mode.
- `.globalPassThrough()` is valid only on scope `0` and should be used for a
  product-wide reserved control.
- `handoffPress(button, scope)` transfers a gesture after an intentional
  authority change.
- `consumePress(button)` explicitly cancels the remaining phases.
- `OverlayManager` automatically quarantines held buttons on an actual
  authority transition, including transitions initiated through its underlying
  visibility stack. New mutating code should still use `OverlayManager`;
  direct stack access is intended for observation and compatibility only.

Do not register the same physical action as both contextual and global. A
reserved global control must have one semantic everywhere; contextual actions
must use another visible binding.

## Encoder boundary

Encoder turns do not have press/release phases, so they are not captured by the
eight-rule button policy. Each turn is resolved immediately against current
authority using the existing scoped/global encoder routing. Modifier buttons
used by encoder bindings still follow the physical-button rules above.

Extending fail-closed global pass-through semantics to encoders is a separate
API and migration decision; it must not be inferred from
`GestureRoutingPolicy::PressScoped`.

## Required verification

At minimum, applications using strict routing should cover:

- opening and closing overlay release quarantine;
- explicit handoff;
- predicate changes during a hold;
- long press, double tap, and combo route stability;
- unbound active-scope consumption;
- ambiguity diagnostics and explicit priority;
- each declared global pass-through control;
- unchanged encoder navigation through nested overlays.
