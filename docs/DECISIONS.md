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