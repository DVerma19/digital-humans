# Biomes

> **Status:** Frozen baseline
> **Depends on:** `WORLDGEN.md` §2–§3, `HYDROLOGY.md`
> **Supersedes:** nothing

This document defines biome identity, biome classification, and
fertility. Biomes are derived from environmental fields, never assigned
by hand, and never from a colored tile lookup.

Nothing here defines the visual appearance of a biome. That belongs in
`RENDERING.md`.

---

## 1. Principle

A biome is a **classification of an environmental vector**, not a
painted region. Every cell belongs to exactly one biome. Biome
boundaries are the natural result of field transitions, not a discrete
mask.

Transition zones are handled by **weights**, not by a separate biome
type. A cell has one primary biome and up to three secondary biomes
with weights.

---

## 2. Classification Inputs

Per LOD 0 cell, biome classification reads:

| Field | Source | Range |
|---|---|---|
| `temperature` | `WORLDGEN.md` L1 `temperature_base` + elevation correction | °C |
| `rainfall` | `WORLDGEN.md` L1 `rainfall_base` | mm/yr |
| `elevation` | `WORLDGEN.md` L2 | meters |
| `slope` | `WORLDGEN.md` L2 | degrees |
| `river_flow` | `HYDROLOGY.md` §3.2 | [0, 1] |
| `lake_depth` | `HYDROLOGY.md` §3.2 | meters |
| `land_mask` | `WORLDGEN.md` L1 | [0, 1] |

No other input affects classification. Adding an input requires a
version bump.

---

## 3. Biome IDs

Biome IDs are stable. They are assigned once and never reused or
renumbered.

| ID | Name | Typical conditions |
|---|---|---|
| 0 | `OCEAN` | `land_mask < ocean_threshold` |
| 1 | `LAKE` | `lake_depth > 0` |
| 2 | `BEACH` | Coastal, low elevation, low slope |
| 3 | `DESERT` | High temperature, very low rainfall |
| 4 | `SAVANNA` | High temperature, moderate rainfall |
| 5 | `TROPICAL_FOREST` | High temperature, high rainfall |
| 6 | `GRASSLAND` | Moderate temperature, low-moderate rainfall |
| 7 | `TEMPERATE_FOREST` | Moderate temperature, moderate rainfall |
| 8 | `BOREAL_FOREST` | Low temperature, moderate rainfall |
| 9 | `TUNDRA` | Very low temperature, low rainfall |
| 10 | `SNOW` | Very low temperature, high elevation |
| 11 | `MOUNTAIN` | High elevation, high slope |
| 12 | `WETLAND` | Low slope, high rainfall, near water |
| 13 | `RIVERBANK` | Adjacent to `river_flow > riverbank_threshold` |

IDs 14–255 are reserved.

---

## 4. Classification Order

Biome classification is applied in this exact order. The first rule
that matches wins.

```
1. OCEAN       if land_mask < ocean_threshold
2. LAKE        if lake_depth > 0
3. SNOW        if elevation > snow_line and temperature < 0
4. MOUNTAIN    if elevation > mountain_line or slope > steep_threshold
5. BEACH       if land_mask < beach_threshold and slope < flat_threshold
6. RIVERBANK   if river_flow > riverbank_threshold
7. WETLAND     if rainfall > wetland_rain and slope < flat_threshold
8. DESERT      if temperature > hot and rainfall < arid
9. SAVANNA     if temperature > hot and rainfall < savanna_rain
10. TROPICAL_FOREST if temperature > hot and rainfall >= savanna_rain
11. GRASSLAND  if rainfall < forest_rain
12. TEMPERATE_FOREST if temperature > cold and rainfall >= forest_rain
13. BOREAL_FOREST if temperature > freezing and rainfall >= forest_rain
14. TUNDRA     otherwise
```

All thresholds are data in `BIOME_PARAMS.md`. This document defines
order and structure, not numbers.

**The order is part of the contract.** Reordering rules changes every
cell's biome and invalidates every chunk hash.

---

## 5. Transition Weights

A cell has one primary biome and up to three secondary biomes.

Weights are computed by:

1. Classify the cell (primary biome).
2. Classify the **same cell** at four offset positions:
   `(x + d, z)`, `(x - d, z)`, `(x, z + d)`, `(x, z - d)`, where
   `d = transition_radius` cells.
3. Any biome that appears in the offsets and differs from the primary
   becomes a secondary biome.
4. Weight is a function of how close the offset biome is and how many
   offsets produced it. Exact formula in `BIOME_PARAMS.md`.

Rules:

1. Weights sum to 1.0.
2. Primary biome weight is always the largest.
3. At most three secondary biomes are stored. Ties break by lower biome ID.
4. Transition radius is a constant, not a per-cell value.

---

## 6. Fertility

Fertility is a continuous field in [0, 1]. It is **not** a biome
property; it is derived from biome, slope, and water proximity.

| Input | Effect |
|---|---|
| Primary biome | Base fertility from `BIOME_PARAMS.md` |
| Slope | Reduced by slope; steeper = less fertile |
| River proximity | Increased near `river_flow > 0` |
| Lake proximity | Increased near `lake_depth > 0` |
| Temperature | Reduced outside a defined comfortable band |

Fertility is a pure function of those five inputs. No randomness.

---

## 7. Determinism Rules

These rules protect G2, G3, and G7.

1. Biome classification is a pure function of the input fields.
2. Input fields are pure functions of `(WorldFrame, world_position)`.
3. The classification order in §4 is fixed. No dynamic reordering.
4. Transition weight computation is deterministic; offsets are
   evaluated in a fixed order.
5. Fertility is a pure function of its five inputs.
6. Biome and fertility contribute to the L2 chunk hash (see §9).
7. No biome boundary uses noise of its own. Jitter, if needed, uses the
   `BIOME_JITTER` noise field (ID 8 in `NOISE.md`), sampled in world
   coordinates.

---

## 8. LOD Rule

Biome classification at LOD N uses the same rule order but samples its
inputs at the LOD N cell center.

Consequence: a biome boundary may shift slightly across LODs. This is
acceptable because:

- The simulation only reads LOD 0 for active chunks.
- Rendering blends across LODs (policy in `RENDERING.md`).
- The biome ID at LOD 0 is authoritative.

LOD is a query resolution, not a new classification.

---

## 9. Hash

Biome and fertility contribute to the L2 chunk hash (see `WORLDGEN.md` §5).

Canonical bytes, in this order:

1. `biome_id` — `u16` LE, `64 × 64` array, row-major
2. `biome_secondary_ids` — `u16[3]` LE, `64 × 64 × 3` array
3. `biome_weights` — `f32[4]` LE bits, `64 × 64 × 4` array
4. `fertility` — `f32` LE bits, `64 × 64` array

No padding. No reordering.

---

## 10. Out of Scope

This document does **not** cover:

- Threshold constants and weight formula → `BIOME_PARAMS.md`
- Visual representation → `RENDERING.md`
- Vegetation types derived from biome → `ECOLOGY.md`
- Wildlife populations → `ECOLOGY.md`
- Resource distribution → `RESOURCES.md`
- Settlement suitability → `SUITABILITY.md`

---

## 11. Open Questions

None. **This document is frozen.**