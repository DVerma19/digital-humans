# Noise

> **Status:** Frozen baseline
> **Depends on:** `WORLDGEN.md` §4, §6
> **Supersedes:** nothing

This document defines how noise is specified, seeded, sampled, and
hashed. Every field in `WORLDGEN.md` is a pure function of noise plus
a small amount of post-processing. This document makes that function
deterministic and portable.

Nothing here defines specific field parameters (frequencies, octaves).
Those belong in `NOISE_PARAMS.md`, which is data, not contract.

---

## 1. Library

**FastNoise2**, MIT license, pinned tag via FetchContent.

No other noise library is permitted. If a feature is missing, it is
written as post-processing on top of FastNoise2, not by swapping the
library.

---

## 2. Field IDs

Every noise field has a stable numeric ID. IDs are assigned once and
never reused or renumbered.

| ID | Name | Used by |
|---|---|---|
| 1 | `LAND_MASK` | L1 land mask |
| 2 | `ELEVATION_BASE` | L1 base elevation |
| 3 | `TEMPERATURE_BASE` | L1 base temperature |
| 4 | `RAINFALL_BASE` | L1 base rainfall |
| 5 | `ELEVATION_DETAIL` | L2 local fractal detail |
| 6 | `ROUGHNESS` | L2 roughness |
| 7 | `RIVER_WARP` | L2 hydrology warping |
| 8 | `BIOME_JITTER` | L2 biome boundary jitter |
| 9 | `VEGETATION_DENSITY` | L3 vegetation density |
| 10 | `SURFACE_OFFSET` | L3 surface offset |

New fields take the next free ID. Retired fields keep their ID forever.

---

## 3. Stream Seeding

Each field has exactly one PCG64 stream per world frame.

```
stream_seed = PCG64(
    hash64(world_seed, generation_version, field_id)
)
```

Where `hash64` is the low 64 bits of `BLAKE3` over the canonical bytes:

```
hash_input = LE64(world_seed) || LE16(generation_version) || LE16(field_id)
```

Rules:

1. One stream per `(world_seed, generation_version, field_id)`.
2. Streams are never shared between fields.
3. Streams are never reused across worlds.
4. Stream state is never stored. It is derived on first use.
5. The stream is used **only** for the noise field's construction.
   Post-processing does not draw from it.

---

## 4. Sampling Convention

Noise is sampled in **world coordinates**, in **meters**, using `f64`
inputs.

```
sample(field_id, world_pos) -> f32
```

Rules:

1. The input is the world position from `COORDINATES.md`.
2. The input is never chunk-local, never render-space, never LOD-scaled.
3. The sample is a pure function of `(world_seed, generation_version,
   field_id, world_pos)`.
4. Sampling order does not affect the result.
5. Sampling on a different thread does not affect the result.
6. Sampling the same position twice returns the same bits.

---

## 5. LOD Rule

A field sampled at LOD N must return the **same value** as the same
world position sampled at LOD 0.

This is achieved by:

- LOD N samples the field at the **center of the LOD-N cell**.
- LOD N does **not** re-generate the field at lower frequency.
- The value at the center of an LOD-N cell is identical to the value
  the LOD 0 cell at that center would produce.

Consequence: LOD is a **query resolution**, not a different field.

If a future field needs different content per LOD (for example, a
silhouette simplification), it is a **new field with a new ID**, not a
change to this rule.

---

## 6. Precision

| Stage | Type | Reason |
|---|---|---|
| World position input | `f64` | Avoids banding at world scale |
| Internal noise accumulators | `f64` | FastNoise2 supports both; f64 is required here |
| Output value | `f32` | Sufficient for all L2/L3 fields |
| Hashing input | `f32` bits, canonical | Deterministic across machines |

No field may accept `f32` world positions. No field may output `f64`.

---

## 7. Determinism Rules

These rules protect G2 and G3.

1. Every sample is a pure function of `(world_seed, generation_version,
   field_id, world_pos)`.
2. No global state, no time, no thread ID, no memory address, no uninitialized
   memory affects any sample.
3. FastNoise2 is used in deterministic mode (no SIMD-dependent output
   differences). If a build produces different bits across machines, that
   build is rejected.
4. Floating-point operations are performed in the order specified by
   FastNoise2's API. No reassociation, no `-ffast-math`.
5. Compilation flags must not enable `-ffast-math`, `-Ofast`, or any flag
   that permits FP reassociation. `CMakeLists.txt` enforces this.

---

## 8. Hash

For chunk hashing (see `WORLDGEN.md` §5), the noise output bytes are
canonical:

- Field values in field-ID order.
- `f32` bit patterns, little-endian.
- No padding between fields.

The chunk hash is `BLAKE3(canonical_bytes(L2 fields))`. L1 is hashed
once per world with the same rules.

---

## 9. Out of Scope

This document does **not** cover:

- Specific frequencies, octaves, lacunarity, gain → `NOISE_PARAMS.md`
- Domain warping parameters → `NOISE_PARAMS.md`
- Field combination formulas (how ELEVATION_BASE and ELEVATION_DETAIL
  combine) → `WORLDGEN.md` §3 or `NOISE_PARAMS.md`
- Hydrology-specific warping → `HYDROLOGY.md`
- Biome jitter semantics → `BIOMES.md`

This document only defines the library, IDs, seeding, sampling, LOD rule,
precision, determinism, and hash. Nothing else.

---

## 10. Open Questions

None. **This document is frozen.**