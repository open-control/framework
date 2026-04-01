# Bounded EventBus RFC

## Goal

Make the framework event bus substantially more deterministic and memory-bounded
without materially degrading the ergonomics of the existing API:

- keep `on() / off() / emit()` as the dominant user-facing shape
- avoid heap-backed internal storage
- preserve safe unsubscribe behavior during `emit()`
- fail clearly when configured limits are exceeded

This RFC focuses on the internal storage backend of `EventBus`, not on a full
rethink of the event API.

## Current Situation

The current implementation is in:

- `src/oc/core/event/EventBus.hpp`
- `src/oc/core/event/EventBus.cpp`
- `src/oc/interface/IEventBus.hpp`

Today it uses:

- `std::unordered_map<uint32_t, std::vector<Subscription>> subscriptions_`
- `std::function<void(const Event&)>` for callbacks
- dead-entry marking with later compaction

This gives decent convenience, but it has several drawbacks on embedded targets:

- dynamic allocation when new event topics appear
- dynamic allocation when a topic vector grows
- hash map overhead and non-deterministic bucket behavior
- less predictable RAM usage
- less predictable boot/runtime behavior
- harder reasoning when failures occur under memory pressure

## Why This Is Worth Doing

Compared to some other framework internals, `EventBus` is a high-value target:

- it is central infrastructure
- it already has explicit capacity thinking via `MAX_SUBSCRIBERS_PER_EVENT`
- its API can stay almost unchanged
- it can be made much more predictable internally

This is one of the best determinism wins available with low DX impact.

## Design Principle

Keep the public model:

- subscribe by `(category, type)`
- receive a `SubscriptionID`
- unsubscribe by ID
- emit by event object

Change only the storage strategy behind it.

The right outcome is:

- fixed-capacity topics
- fixed-capacity subscribers per topic
- no `unordered_map`
- no per-topic `vector`
- clear overflow diagnostics

## Proposed Backend

Replace:

- `unordered_map<uint32_t, std::vector<Subscription>>`

with a fixed-capacity topic table.

### New Config Limits

Add a new config constant:

- `OC_MAX_EVENT_TOPICS`

Suggested default:

- `32`

Keep:

- `OC_MAX_SUBSCRIBERS_PER_EVENT`

This gives a bounded total event-bus capacity of:

- `MAX_EVENT_TOPICS * MAX_SUBSCRIBERS_PER_EVENT`

### Internal Structures

Suggested shape:

```cpp
struct SubscriptionSlot {
    SubscriptionID id = 0;
    EventCallback callback{};
    bool alive = false;
};

struct TopicSlot {
    uint32_t key = 0;
    bool used = false;
    std::array<SubscriptionSlot, MAX_SUBSCRIBERS_PER_EVENT> subscribers{};
};
```

Inside `EventBus`:

```cpp
std::array<TopicSlot, MAX_EVENT_TOPICS> topics_{};
SubscriptionID next_id_ = 1;
size_t dead_count_ = 0;
Stats stats_{};
```

## Core Behaviors

### Topic lookup

Lookup becomes linear over `MAX_EVENT_TOPICS`.

That is acceptable because:

- event-topic cardinality is expected to stay low
- `MAX_EVENT_TOPICS` is small and explicit
- deterministic bounded linear scan is preferable to hash-map allocation behavior

Helper methods:

- `findTopic_(key)`
- `findOrCreateTopic_(key)`

### Subscribe

`on(category, type, callback)` should:

1. reject empty callbacks
2. find or allocate the topic slot
3. count active subscribers for that topic
4. reject if `MAX_SUBSCRIBERS_PER_EVENT` is reached
5. reuse dead subscriber slots when possible
6. otherwise use the first empty subscriber slot
7. return `0` on failure with a clear log

### Emit

`emit(event)` should:

1. find the topic slot
2. iterate all subscriber slots
3. invoke only `alive` subscribers with non-empty callbacks

No copying should be needed.

### Unsubscribe

`off(id)` should:

1. linearly scan all topic slots and subscriber slots
2. if found and alive:
   - mark dead
   - clear callback storage
   - increment `dead_count_`
3. remain idempotent

This preserves current semantics and keeps `emit()` safe while iterating.

### Compaction

Compaction should become cheap and local:

- not erase memory
- simply normalize topic subscriber slots
- optionally clear empty topic slots

There are two valid strategies.

#### Strategy A: no compaction of topic arrays

- dead slots are simply reused by later subscriptions
- topic slot remains allocated once created

Pros:

- simplest
- very deterministic
- minimal mutation

Cons:

- topic slots never get reclaimed unless `clear()`

#### Strategy B: compact subscribers and reclaim empty topics

- shift alive subscribers toward the front
- clear trailing dead slots
- if topic becomes empty, mark topic as unused

Pros:

- cleaner state
- topic table reuse is better

Cons:

- slightly more logic

Recommendation:

- start with Strategy B
- still keep implementation simple and bounded

## Public API Impact

The public API can remain almost identical:

- `SubscriptionID on(category, type, callback)`
- `void emit(const Event&)`
- `void off(SubscriptionID)`

Optional additions:

- `static constexpr size_t maxTopics()`
- `size_t topicCount() const`
- `size_t subscriberCount(key)` or a debug-only equivalent

These are optional and mostly useful for diagnostics.

## Diagnostics

This refactor is especially valuable if failures become immediately actionable.

Overflow logs should include:

- event key
- category
- type
- active subscriber count
- max per event
- current topic count
- max topics

Examples:

- topic table full
- per-topic subscriber table full

## Memory and Determinism Tradeoff

### Benefits

- fixed memory footprint
- no runtime topic allocation
- no vector growth
- deterministic upper bounds
- simpler failure modes

### Costs

- reserved capacity even for unused topics
- linear scan for topic lookup
- possible increase in baseline RAM depending on chosen limits

This is a good trade if:

- `MAX_EVENT_TOPICS` stays modest
- event bus is treated as infrastructure, not arbitrary dynamic messaging

## DX Impact

Very small if we keep the public API intact.

What should remain unchanged:

- lambdas at call sites
- `on()/off()/emit()` signatures
- unsubscribe by ID

What changes:

- overflow becomes explicit and predictable
- topic count becomes a configured concept

Main DX risk:

- code that silently relied on “unbounded enough” event topics may now hit a hard limit

Mitigation:

- conservative default capacity
- excellent error logs
- expose config constants clearly

## Relationship to InplaceFunction

This RFC stands on its own.

However, it composes well with a future callback refactor:

- once `EventBus` storage is bounded
- `EventCallback` can later move from `std::function` to `InplaceFunction`

That would further reduce hidden allocations without changing the event-bus model again.

Recommended sequencing:

1. bounded event-bus backend first
2. callback primitive refactor later

## Proposed Implementation Steps

### Step 0

Add config:

- `OC_MAX_EVENT_TOPICS`
- `MAX_EVENT_TOPICS`

### Step 1

Refactor `EventBus` internals:

- fixed topic array
- fixed subscriber arrays
- linear lookup helpers

### Step 2

Preserve current semantics:

- safe unsubscribe during emit
- idempotent `off()`
- stats tracking

### Step 3

Improve diagnostics:

- logs for topic-table overflow
- logs for per-topic subscriber overflow

### Step 4

Add tests:

- subscribe and emit
- multiple topics
- per-topic capacity overflow
- topic-table capacity overflow
- unsubscribe during emit
- idempotent off
- topic reuse after compaction/clear

## Consumer Impact

### `midi-studio/core`

High value, low API migration burden.

Benefits:

- safer runtime infrastructure
- clearer failures during boot/context wiring
- bounded memory behavior

### `midi-studio/plugin-bitwig`

Also a good beneficiary.

Even though host data can vary, the number of event topics should remain
structurally bounded by the application, not by host list sizes.

### `open-control/note`

Low impact, but no downside.

## Recommendation

This should be treated as a **must-do** determinism optimization.

It has:

- high infrastructure value
- strong determinism benefit
- low DX disruption
- clear failure semantics

It is a better next target than broad speculative refactors because the blast
radius is controlled and the payoff is concrete.
