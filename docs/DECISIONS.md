# Decisions

## 2026-09-22 — Noise implementation

**Decision:** Custom value-noise implementation, no third-party noise library.

**Alternatives:** FastNoise2 (frozen in NOISE.md v1).

**Reason:** FastNoise2 uses runtime SIMD dispatch, which can produce different
floating-point bits across CPUs. `DETERMINISM.md` §6 forbids this. A custom
integer-lattice value-noise implementation gives bit-identical output on any
machine, is fully inspectable, and adds no dependencies.

**Reversibility:** Reversible. FastNoise2 can be added behind the same API
if performance ever requires it.

**Related:** NOISE.md §1, §3 (amended), DETERMINISM.md §6.

## 2026-09-22 — Golden hash protocol

**Decision:** Chunk hashes are pinned in `tests/chunk_test.cpp` as string
constants. If the pinned test fails on a later commit, the failure is treated
as a real regression.

**Alternatives considered:** Recompute golden hashes on every run (no — that
defeats the purpose). Store hashes in an external file (no — must be visible
in code review).

**Reason:** Determinism across machines and commits is the core contract of
the project. A frozen hash is the only honest way to enforce it. Any change
to noise, chunk generation, canonical byte layout, or compiler FP behavior
will be caught.

**Procedure if the golden test fails:**

1. Do NOT edit the expected hashes.
2. Identify which commit changed the behavior.
3. Decide: is the change intended? 
   - If intended (e.g. a deliberate noise change), update the hashes in a
     dedicated commit whose message explains why.
   - If unintended, revert the offending change.
4. Record the decision in this file.

**Reversibility:** The protocol is reversible (change the test), but every
change to the golden hashes must be accompanied by a decision entry.

**Reference hashes:**
- chunk(0,0) seed=42 v=1: a4194e572772056f2f818d239f51b87a9846eac7e7449d6f2d00e2d14de043e7
- chunk(1,0) seed=42 v=1: afd4d7eaa12fe57dbbf9e57fdc888fd7bd8cbb7cfdf7e0f629c7dd2a1038d25a
- chunk(0,1) seed=42 v=1: b9ec8f020aa8ec55238b0f065ac11dc0a1bd225e35f10dae2a61695bfcb3dc90
- chunk(0,0) seed=99 v=1: 09ddb0d63fa0d59fad31760a84b5c36fb0f6c21bb19b7095135c74339969a88b
- chunk(0,0) seed=42 v=2: 4cb48a9ea20c1f565ba5a862f4107f3b7eec8b14b1c2b7d0b3ef3e857db9995a