# Determinism

> **Status:** Frozen baseline
> **Depends on:** `ARCHITECTURE.md` §11.2, `WORLDGEN.md` §4, `NOISE.md` §3, §7
> **Supersedes:** nothing

This document defines the rules that make the simulation produce
identical state hashes regardless of thread count, evaluation order,
or machine. Breaking any rule here breaks G2 and every downstream gate.

Nothing here defines the threading model, the job system, or the
scheduler. Those belong in `SCHEDULER.md`.

---

## 1. The One Rule

The same `(seed, generation_version, ruleset_version, tick_count,
worker_count)` produces the same state hash. Every rule below exists to
preserve this.

---

## 2. Sources of Non-Determinism

Five things can break determinism. Each has exactly one fix.

| Source | Fix |
|---|---|
| Shared mutable state across threads | Per-tick ordered commit queue |
| RNG shared across systems or threads | One stream per `(system, tick, entity)` |
| Floating-point reassociation | Forbidden compiler flags; fixed evaluation order |
| Iteration over unordered containers | Sorted iteration, or a stable hash-based order |
| Time, thread ID, address, uninitialized memory | Banned from simulation and generation code |

If any of these appear in code, the code is wrong. This document does
not permit exceptions.

---

## 3. Commit Order

Workers may compute in any order. All commits go through one ordered
queue per tick.

```
commit_key = (tick, system_priority, entity_id, event_sequence)
```

- `tick` — simulation tick (u64, monotonically increasing)
- `system_priority` — fixed integer per system, see §4
- `entity_id` — u64, unique per entity, assigned at creation
- `event_sequence` — u32, per-entity sequence within the tick

Rules:

1. Workers submit results to the queue.
2. The queue drains in `commit_key` order on a single thread.
3. No system may read mutable state written by another system in the
   same tick.
4. A system that must read another system's output reads it from the
   previous tick's committed state.

---

## 4. System Priorities

Fixed integer per system. Lower runs first. Once assigned, a priority
is never changed.

| Priority | System |
|---|---|
| 10 | Needs decay |
| 20 | Movement |
| 30 | Local interaction |
| 40 | Production |
| 50 | Exchange |
| 60 | Household |
| 70 | Relationships |
| 80 | Belief revision |
| 90 | Goal revision |
| 100 | Events |
| 110 | Statistics, diagnostics |

Generation systems (chunk base) do not participate in the tick queue.
They are pure functions of `(WorldFrame, world_position)` and have no
commit order.

---

## 5. RNG Streams

Every RNG draw comes from a PCG64 stream identified by:

```
stream_key = (world_seed, generation_version, ruleset_version, system_id, tick, entity_id)
```

Rules:

1. One stream per `stream_key`. Streams are never shared.
2. Streams are never reused across ticks.
3. Stream state is never stored in serialized state.
4. Streams are derived on first draw. The derivation is:
   ```
   seed64 = hash64(canonical_bytes(stream_key))
   ```
   where `hash64` is BLAKE3 truncated to 64 bits.
5. A system may not draw from another system's stream.
6. A draw from a stream advances only that stream. No global RNG.

**World generation is not part of the tick system.** It uses its own
streams, defined in `NOISE.md` §3.

---

## 6. Floating-Point Rules

1. `-ffast-math`, `-Ofast`, `-funsafe-math-optimizations`,
   `-freciprocal-math`, and any flag that permits FP reassociation are
   **forbidden project-wide**. `CMakeLists.txt` enforces this.
2. `#pragma STDC FP_CONTRACT OFF` is required in every translation unit
   that performs floating-point accumulation.
3. Accumulations must use the order specified by the owning document
   (e.g. `HYDROLOGY.md` §2.3).
4. `f64` is used for authoritative world positions and for noise
   internals. `f32` is used for stored field values. No implicit
   widening or narrowing without an explicit cast.
5. The canonical hash of any `f32` value is its raw bit pattern,
   little-endian, no normalization.

---

## 7. Container Iteration

1. `std::unordered_map`, `std::unordered_set`, and any hash container
   are **forbidden** in simulation state and in generation state.
2. Iteration over `std::map`, `std::set`, and sorted `std::vector` is
   allowed and is deterministic.
3. If a hash-based lookup is needed for performance, the container is
   not iterated. Only lookup is permitted, and results are sorted before
   any output that affects state.
4. Serialization always writes fields in canonical order, never
   container iteration order.

---

## 8. Worker Rules

1. Workers may compute anything that is a pure function of committed
   state and their own RNG stream.
2. Workers submit to the commit queue. They do not write shared state.
3. Worker count must not affect the result. `N = 1` and `N = 16` produce
   identical hashes.
4. A system that must write to a shared buffer writes through the
   commit queue, never directly.
5. Parallel work that produces a set of values must produce them in
   canonical order, not in completion order.

---

## 9. Hash

The **state hash** at tick T is:

```
state_hash(T) = BLAKE3( canonical_bytes(committed_state_at_T) )
```

Canonical bytes:

- Fields written in a fixed order defined by the owning document.
- Little-endian throughout.
- No padding between fields.
- Arrays written in canonical order, not container iteration order.

Hash functions, serialization rules, and canonical byte layouts are
per-document. This document defines only the guarantees: same inputs →
same bytes → same hash.

---

## 10. Test Requirements (G2)

G2 must verify **all** of the following:

1. Same seed, same version, one worker: two runs produce identical
   chunk hashes.
2. Same seed, same version, N workers: N runs produce identical chunk
   hashes.
3. Same seed, same version, 1 worker vs N workers: identical hashes.
4. Different seed: different hashes.
5. Different generation version: different hashes.
6. Same seed, different machine (if available in CI): identical hashes.

If any of these fail, no further system may be committed.

---

## 11. Out of Scope

This document does **not** cover:

- Thread pool design, job graph → `SCHEDULER.md`
- Serialization formats → `SAVE_FORMAT.md`
- Canonical field order per system → the owning doc for that system
- Replay checkpoint format → `SAVE_FORMAT.md`
- Compiler flags beyond FP → `BUILD.md`

---

## 12. Open Questions

None. **This document is frozen.**