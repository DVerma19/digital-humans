# Hydrology

> **Status:** Frozen baseline
> **Depends on:** `ARCHITECTURE.md` §6.1, `WORLDGEN.md` §2–§3, `NOISE.md`
> **Supersedes:** nothing

This document defines how water is represented, computed, and queried.
Hydrology is an **authoritative L2 layer**, not a visual overlay. It
must be queryable by simulation, terrain shaping, biome classification,
and rendering alike.

Nothing here defines the visual appearance of water. That belongs in
`RENDERING.md`.

---

## 1. Why Hydrology Is Two-Pass

Drainage is a **global** property: water flows downhill across chunk
boundaries, and a cell's river width depends on accumulation upstream,
which may be hundreds of chunks away.

Chunk generation is **local**: a chunk cannot see the whole world.

Resolution: hydrology is computed in two passes.

| Pass | Scope | When | Frequency |
|---|---|---|---|
| **Pass 1 — Basin** | Whole world, coarse grid | Once per world load | Once |
| **Pass 2 — Refine** | Per chunk, at LOD 0 | On chunk generation | Per chunk |

Pass 1 gives every chunk its basin context. Pass 2 refines within that
context. Seams are guaranteed because both passes agree at boundaries
**by construction**, not by tolerance.

---

## 2. Pass 1 — Basin

### 2.1 Basin grid

A coarse grid covering the world at a fixed resolution.

| Property | Value |
|---|---|
| Resolution | `128 × 80` cells (`CHUNK_COUNT_X × CHUNK_COUNT_Z`) |
| Cell size | `64 m` (same as `CHUNK_SIZE_XZ`) |
| Origin | World frame origin, aligned with chunk addresses |
| Stored | Once per world, in world metadata |

The basin grid is **one cell per chunk**. It is not a per-cell field.

### 2.2 Basin fields

| Field | Type | Range | Meaning |
|---|---|---|---|
| `basin_id` | `u32` | `> 0` | Drainage basin identifier; `0` = endorheic / none |
| `basin_direction` | `u8` | 0–7 | Direction of flow out of this cell, one of 8 neighbors |
| `basin_accum` | `u32` | cells | Upstream cell count including self |
| `basin_order` | `u8` | 0–12 | Strahler order of the river segment in this cell |
| `basin_is_lake` | `bool` | — | True if cell is a lake |
| `basin_is_ocean` | `bool` | — | True if cell is ocean |

### 2.3 Basin algorithm

Computed once at world load, in this order:

1. Sample `elevation_base` at the center of each basin cell.
2. Sample `land_mask` at the center of each basin cell.
3. Mark cells with `land_mask < ocean_threshold` as ocean.
4. For every land cell, set `basin_direction` to the steepest-descent
   neighbor (D8). Ocean cells have no direction.
5. Compute `basin_accum` by processing cells in descending elevation
   order and propagating accumulation downstream.
6. Mark depressions (cells whose neighbors are all higher) as lakes
   if their accumulation exceeds a minimum. Recompute downstream flow
   to exit through the lowest rim of the depression.
7. Assign `basin_id` by flood-filling downstream from each outlet.
8. Compute `basin_order` (Strahler) by processing in ascending
   accumulation order.

This is a standard D8 + priority-flood depression handling pipeline.
The exact thresholds are data in `HYDROLOGY_PARAMS.md`.

### 2.4 Determinism

- Cell processing order is canonical: sort by `(elevation, x, z)`.
- Ties break by `(x, z)` ascending.
- No floating-point arithmetic with non-deterministic order.
- One pass, one thread, one deterministic result.

Pass 1 is **not parallel** in v1. It runs once per world load. Its cost
is negligible compared to chunk generation.

---

## 3. Pass 2 — Refine

### 3.1 Inputs

A chunk at LOD 0 receives:

- Its own L1 fields (`elevation_base`, `rainfall_base`).
- Its own L2 `elevation` (from `WORLDGEN.md` §2).
- The basin cell(s) that overlap it, from Pass 1.

A chunk of `64 × 64` cells overlaps exactly `1 × 1` basin cell in v1.
If a future version increases basin resolution, a chunk may overlap
multiple basin cells and must handle each.

### 3.2 Per-cell fields

For every cell in the chunk, Pass 2 produces:

| Field | Type | Range | Meaning |
|---|---|---|---|
| `flow_direction` | `u8` | 0–7, or `255` | Local D8 direction; `255` = no flow (ocean, lake, pit) |
| `flow_accum` | `f32` | `[0, +∞)` | Upstream area in cells, including self |
| `river_flow` | `f32` | `[0, 1]` | Normalized river strength; 0 = no river |
| `river_order` | `u8` | 0–12 | Local Strahler order |
| `lake_depth` | `f32` | meters | 0 = no lake |

### 3.3 Refinement rules

1. **Local direction** is computed from `elevation` within the chunk
   **plus** the 8-cell halo (see `CHUNKS.md` §4).
2. **Local flow does not change basin membership.** If the basin cell
   says this chunk drains east, every river that exits the chunk must
   exit east.
3. **Flow accumulation at chunk boundary** is read from the upstream
   basin cell's `basin_accum`, scaled to LOD 0 cells. This is the only
   way upstream contribution enters the chunk.
4. **`river_flow`** is derived from `flow_accum` via a monotonic curve
   defined in `HYDROLOGY_PARAMS.md`. It is a pure function, no lookup.
5. **`river_order`** is Strahler within the chunk, seeded by the basin
   cell's `basin_order` at entry points.
6. **`lake_depth`** is set only where the chunk contains a basin cell
   marked `basin_is_lake`. Depth is from a deterministic function of
   basin area, defined in `HYDROLOGY_PARAMS.md`.

### 3.4 Halo requirement

Pass 2 reads a **8-cell halo** of `elevation` around the chunk. This
halo is generated from the base, never from modifications (see
`CHUNKS.md` §4). It is discarded after generation.

The halo is required because local flow direction at the chunk edge
depends on neighbor elevations.

---

## 4. Seam Guarantees

Two adjacent chunks must agree on:

1. **Flow exiting the shared edge.** The direction of a river crossing
   the boundary is the same from both sides.
2. **`river_flow` at the shared edge.** Values match within `1e-6`.
3. **`river_order` at the shared edge.** Exact integer match.
4. **`lake_depth` at a shared lake edge.** Values match within `1e-6`.

These are guaranteed because:

- Basin cell assignment is the same from both sides (Pass 1, global).
- Halo elevation is the same from both sides (pure function of world
  position).
- River flow at the boundary is dominated by upstream basin accumulation,
  which is identical from both sides.

No tolerance is needed at the basin level. Tolerance is only used for
`f32` comparison at the cell level, tested in G6.

---

## 5. Query API

Hydrology is queried by other systems through a small, frozen API.

```cpp
namespace dh::hydro {

    struct HydroSample {
        float flow_accum;
        float river_flow;
        uint8_t river_order;
        float lake_depth;
        uint32_t basin_id;
    };

    HydroSample sample(ChunkAddress c, uint16_t local_x, uint16_t local_z);
    HydroSample sample_world(WorldPos p);

    float distance_to_river(WorldPos p, float max_range_m);
    bool  is_on_river(WorldPos p);
    bool  is_on_lake(WorldPos p);
    uint32_t basin_at(WorldPos p);
}
```

Rules:

1. `sample_world` is a convenience that resolves the chunk address first.
2. `distance_to_river` searches outward up to `max_range_m`. It returns
   `max_range_m` if no river is found within range.
3. All queries are pure. They do not mutate chunk state.
4. All queries are deterministic. Same inputs → same outputs.

---

## 6. Hash

Hydrology contributes to the L2 chunk hash (see `WORLDGEN.md` §5).

Canonical bytes for hydrology, in this order:

1. `flow_direction` — `u8`, `64 × 64` array, row-major
2. `flow_accum` — `f32` LE bits, `64 × 64` array
3. `river_flow` — `f32` LE bits, `64 × 64` array
4. `river_order` — `u8`, `64 × 64` array
5. `lake_depth` — `f32` LE bits, `64 × 64` array

No padding. No reordering.

---

## 7. Determinism Rules

These rules protect G2, G3, and G6.

1. Pass 1 is single-threaded and deterministic. No exceptions.
2. Pass 2 is a pure function of `(WorldFrame, chunk_address, halo_elevation)`.
3. Pass 2 may run in parallel across chunks. Parallel results must
   produce identical bytes to serial results.
4. Pass 2 never reads another chunk's modifications.
5. Pass 2 never reads its own previously cached results.
6. All floating-point operations follow the order specified in
   `HYDROLOGY_PARAMS.md`. No reassociation.
7. `-ffast-math`, `-Ofast`, and any FP-reassociation flags are forbidden
   project-wide. Enforced in `CMakeLists.txt`.

---

## 8. Out of Scope

This document does **not** cover:

- Threshold constants, curve parameters, lake depth formula →
  `HYDROLOGY_PARAMS.md`
- Visual representation of water → `RENDERING.md`
- Water as a resource → `RESOURCES.md`
- River-based transport, fishing, irrigation → `ECONOMY.md`
- Water-driven settlement suitability → `SUITABILITY.md`

---

## 9. Open Questions

None. **This document is frozen.**