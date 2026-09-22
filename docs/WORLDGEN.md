# World Generation

> **Status:** Frozen baseline
> **Depends on:** `ARCHITECTURE.md` §5–§6, `COORDINATES.md`, `CHUNKS.md`
> **Supersedes:** nothing

This document defines the fields the world generator produces, their
types and ranges, the order in which they are computed, and the rules
that make generation deterministic and chunk-addressable.

Nothing here defines the noise algorithm, the hydrology algorithm, or
the biome classification rules. Those belong in `HYDROLOGY.md` and
`BIOMES.md`.

---

## 1. World Frame (L0)

A world is fully identified by:

```
WorldFrame {
    seed               : u64
    generation_version : u16
    profile            : GenerationProfile
}
```

`GenerationProfile` (v1):

| Field | Type | Meaning |
|---|---|---|
| `ocean_ratio` | `f32` | Target fraction of surface that is ocean, [0, 1] |
| `temperature_offset` | `f32` | Shift applied to the temperature field, °C |
| `rainfall_scale` | `f32` | Multiplier on the rainfall field, [0.5, 2.0] |
| `mountain_propensity` | `f32` | Bias toward high-elevation terrain, [0, 1] |

Changing any of these invalidates every chunk. The profile is stored in
the world metadata, never inferred.

---

## 2. Field Layers

Fields are organized by scale. Each field is a **pure function of world
position**, given the world frame. Chunking controls access, not values.

### L1 — Macro (world-scale, coarse)

| Field | Type | Range | Notes |
|---|---|---|---|
| `land_mask` | `f32` | [0, 1] | 0 = ocean, 1 = full land |
| `elevation_base` | `f32` | meters | Before local detail; can be negative below sea level |
| `temperature_base` | `f32` | °C | Function of Z (north–south) and `elevation_base` |
| `rainfall_base` | `f32` | mm/yr | Function of distance to ocean and `elevation_base` |

L1 is computed at a fixed coarse resolution for the whole world once at
load. It is **not** stored per chunk; it is a single world-level artifact.

### L2 — Regional (per chunk cell at LOD 0)

| Field | Type | Range | Notes |
|---|---|---|---|
| `elevation` | `f32` | meters | `elevation_base` + local fractal detail |
| `slope` | `f32` | degrees [0, 90] | Gradient of `elevation` |
| `biome_id` | `u16` | enum | From `temperature_base`, `rainfall_base`, `elevation` |
| `fertility` | `f32` | [0, 1] | From biome, slope, river proximity |
| `roughness` | `f32` | [0, 1] | Local variation of elevation |
| `river_flow` | `f32` | [0, 1] | Fractional flow accumulation; 0 = no river |
| `lake_depth` | `f32` | meters | 0 = no lake |

L2 is the **authoritative base** of a chunk (see `CHUNKS.md` §5). It is
generated on demand, never stored, and its hash is the chunk hash.

### L3 — Local (per cell, only at LOD 0, near camera)

| Field | Type | Range | Notes |
|---|---|---|---|
| `material_id` | `u8` | enum | Surface material (rock, soil, sand, …) |
| `vegetation_type` | `u8` | enum | From biome + fertility |
| `vegetation_density` | `f32` | [0, 1] | From biome + fertility + slope |
| `surface_offset` | `f32` | meters | Small local elevation detail |

L3 is **derived cache** (see `CHUNKS.md` §5). It is deterministic, but it
is not part of the chunk hash, and it is not generated for distant chunks.

---

## 3. Generation Order

Every field may depend only on fields computed earlier in this list.

```
1. World frame           (seed, version, profile)
2. L1 land_mask
3. L1 elevation_base
4. L1 temperature_base
5. L1 rainfall_base
6. L2 elevation
7. L2 slope
8. L2 hydrology          (river_flow, lake_depth)   -- see HYDROLOGY.md
9. L2 biome_id
10. L2 fertility
11. L2 roughness
12. L3 material_id
13. L3 vegetation_type
14. L3 vegetation_density
15. L3 surface_offset
```

Steps 8–15 may be recomputed lazily. Steps 2–5 are computed once.

---

## 4. Determinism Rules

These rules protect G2 and G3. Breaking any of them breaks the project.

1. Every field is a pure function of `(WorldFrame, world_position)`.
   No global state, no time, no order dependence.
2. All randomness is derived from a single root: `seed`. No other source
   of entropy is permitted in world generation.
3. Each field draws from its own PCG64 stream, seeded as:
   ```
   stream_seed = hash(world_seed, generation_version, field_id)
   ```
   Fields never share a stream.
4. The same `(WorldFrame, world_position)` produces the same value on
   any machine, any thread count, any order of evaluation.
5. LOD sampling of an L2 field must reproduce the LOD 0 value at that
   cell. LOD is a query, not a re-generation.
6. Halo reads must not mutate the read chunk (see `CHUNKS.md` §4).

---

## 5. Hash

The **chunk hash** (used in G2) is:

```
chunk_hash = BLAKE3( canonical_bytes( L2 fields for this chunk at LOD 0 ) )
```

Canonical bytes:

- Field order: exactly the order listed in §2 (L2).
- Byte order: little-endian.
- Padding: none.
- Layout: struct-of-arrays, one array per field, in field order.

The hash covers L2 only. L1 is world-level and hashed once per world.
L3 is derived and not hashed.

---

## 6. Libraries

| Concern | Choice | License | Acquisition |
|---|---|---|---|
| Noise | FastNoise2 | MIT | FetchContent, pinned tag |
| Hash | BLAKE3 (reference impl) | CC0 / Apache-2.0 | FetchContent, pinned tag |
| RNG | PCG64 (reference impl) | Apache-2.0 | Vendored in `third_party/pcg/` |

No other noise or hash library is permitted. All three are added in the
same CMake commit that first needs them.

---

## 7. Out of Scope

This document does **not** cover:

- Noise algorithm and parameters → `NOISE.md`
- Hydrology algorithm → `HYDROLOGY.md`
- Biome classification rules → `BIOMES.md`
- Resource distribution → `RESOURCES.md`
- Settlement suitability → `SUITABILITY.md`
- LOD sampling policy → `RENDERING.md`
- Modification format → `SAVE_FORMAT.md`

---

## 8. Open Questions

None. **This document is frozen.**