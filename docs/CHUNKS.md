# Chunks

> **Status:** Frozen baseline
> **Depends on:** `ARCHITECTURE.md` §6, `COORDINATES.md`
> **Supersedes:** nothing

This document defines what a chunk is, how it is addressed, what it
contains, how it is loaded and unloaded, and the rules that keep
generation deterministic and seams invisible.

Nothing in this document defines memory layout or file format. Those
belong in `SAVE_FORMAT.md` and the implementation.

---

## 1. What a Chunk Is

A **chunk** is a fixed-size spatial unit used by three systems:

- **Generation** — the smallest unit the generator produces.
- **Streaming** — the smallest unit loaded, unloaded, and cached.
- **Persistence** — the smallest unit stored and recovered.

A chunk is not a render object. A chunk is not a simulation entity.
It is the shared spatial contract between world generation, streaming,
persistence, and rendering.

---

## 2. Geometry

A chunk is a vertical column over a horizontal square.

| Property | Value | Notes |
|---|---|---|
| Horizontal size | `64 × 64` cells | Matches `CHUNK_SIZE_XZ` in `COORDINATES.md` |
| Vertical extent | Full terrain column | From world floor to world ceiling |
| Cell size at LOD 0 | `1 m` | Native simulation resolution |
| Chunk world size | `64 m × 64 m` | At LOD 0 |

Vertical subdivision (caves, underground layers) is **not** part of v1.
If added later, it becomes a new chunk dimension, not a mutation of this one.

---

## 3. Addressing

Chunk addressing is defined in `COORDINATES.md` §5 and §6.

- Address type: `ChunkAddress { int32 x, z }`
- Origin: world frame origin
- Conversion: `world_to_chunk` (floor division)
- World bounds (v1): `CHUNK_COUNT_X = 128`, `CHUNK_COUNT_Z = 80`

Addresses outside the world bounds are invalid. No wrapping.

---

## 4. Halo

Adjacent chunks must agree at their shared boundary. To compute a
boundary-correct chunk, the generator needs neighbor data.

| Pass | Halo radius | Reason |
|---|---|---|
| Elevation (pure noise) | 0 cells | Function of world coord only |
| Erosion / flow refinement | 8 cells | Local neighborhood affects local shape |
| Biome blending | 4 cells | Smooth transitions across boundaries |
| Runtime queries | 0 cells | All cross-chunk reads go through the chunk manager |

**Rules:**

1. Halo is **read-only** and **discarded after generation**. It is not
   stored in the chunk.
2. A chunk may never read another chunk's **modifications**. Only base
   procedural data is shared.
3. Halo reads must be pure functions of `(seed, generation_version, world_coord)`.
4. Halo size is a property of each generation pass, not the chunk itself.

---

## 5. Chunk Contents

A chunk contains three layers, in this order:

| Layer | Source | Stored? | Mutable? |
|---|---|---|---|
| **Identity** | Assigned at load | Yes (small header) | No |
| **Base** | Procedural, from `(seed, version, address)` | No | No |
| **Modifications** | Simulation edits | Yes (sparse diff) | Yes |
| **Derived cache** | Computed from base + mods | Rebuildable | Yes (ephemeral) |

### Identity

- `address` — `ChunkAddress`
- `lod` — level of detail (see §6)
- `generation_version` — uint16
- `base_hash` — hash of the procedural base for this address at this version
- `mod_count` — number of modification entries

### Base

Deterministic function of `(seed, generation_version, address)`. Never stored.
Regenerated on demand. Examples: height, material, biome, fertility, water
presence. Exact field list belongs in `WORLDGEN.md`.

### Modifications

Sparse diff. Only cells changed by the simulation. Examples: a tree
harvested, a road built, a building placed. Format belongs in
`SAVE_FORMAT.md`.

### Derived cache

Rebuildable at any time from base + mods. Examples: render mesh, collision
mesh, visibility bounds. Never persisted. Discarded on unload.

---

## 6. LOD Levels

Levels of detail are powers of two. LOD only affects **visual and query
resolution**, never the authoritative simulation.

| LOD | Samples | Cell size | Use |
|---|---|---|---|
| 0 | 64 × 64 | 1 m | Near camera, simulation-active |
| 1 | 32 × 32 | 2 m | Mid distance |
| 2 | 16 × 16 | 4 m | Far distance |
| 3 | 8 × 8 | 8 m | Very far |
| 4 | 4 × 4 | 16 m | Macro silhouette |

Beyond LOD 4, the macro map handles the view. No chunk is generated.

**Rules:**

1. A chunk at any LOD has the same address and the same identity header.
2. LOD selection is a policy, not part of the chunk contract. The mapping
   from distance to LOD belongs in `RENDERING.md`.
3. LOD transitions must not pop. Blending policy belongs in `RENDERING.md`.
4. The simulation always reads LOD 0 data for active chunks, regardless
   of which LOD is being rendered.

---

## 7. Lifecycle

A chunk is always in exactly one of these states:

| State | Meaning |
|---|---|
| **Unloaded** | Not in memory |
| **Loading** | Scheduled; base being computed or mods being applied |
| **Ready** | In memory, not active |
| **Active** | In memory, currently used by simulation or rendering |
| **Dormant** | In memory, cached, not active |
| **Unloading** | Being removed |

Allowed transitions:

```
Unloaded  → Loading   (request)
Loading   → Ready     (finish)
Ready     → Active    (activate)
Active    → Dormant   (deactivate)
Dormant   → Active    (reactivate)
Ready     → Unloading (evict)
Dormant   → Unloading (evict)
Unloading → Unloaded  (done)
```

No other transitions are allowed. The chunk manager owns transitions.

---

## 8. Cache

Every loaded chunk has a cache key:

```
CacheKey = (world_seed, generation_version, chunk_address, lod)
```

Rules:

1. Two chunks with the same cache key are byte-identical in base and mods.
2. Changing `generation_version` invalidates every cache entry.
3. Changing `world_seed` invalidates every cache entry.
4. Modifications are keyed by `(world_seed, generation_version, chunk_address)`
   — **not** by LOD. All LODs of the same chunk share the same mods.
5. Derived caches are keyed by the full `CacheKey` and are discarded on
   any invalidation.

---

## 9. Persistence

Three things are stored:

| Item | Content | Size |
|---|---|---|
| Chunk header | Identity fields | Fixed |
| Modifications | Sparse diff per chunk | Proportional to change count |
| Base | Nothing | — |

The base is never stored. It is regenerated from `(seed, generation_version, address)`.

To load a chunk:

1. Compute the procedural base.
2. Apply the stored modifications.
3. Rebuild the derived cache.

**A chunk with zero modifications is byte-identical to a fresh generation
of the same seed and version.** This is a hard requirement. Tested in G2.

---

## 10. Determinism Rules

These rules protect G2 and G3. Breaking any of them breaks the project.

1. A chunk's base is a pure function of `(seed, generation_version, address)`.
   No global state, no order dependence, no time, no randomness outside
   the seed.
2. A chunk's base is independent of what other chunks have been loaded,
   when it was loaded, and in what order.
3. A chunk's base is independent of LOD. LOD samples the same base at
   different resolutions.
4. Modifications are applied in a canonical order (sorted by cell index,
   then by field) so the result is byte-stable.
5. Halo reads must not mutate the read chunk.

---

## 11. Boundary Rules

1. Adjacent chunks must produce the same value at their shared edge for
   any field that claims to be continuous.
2. Seam tolerance for height samples: `1e-6` world units.
3. Seam tolerance for biome classification: exact match at the shared edge.
4. Hydrological features (rivers, basins) follow §6.1 of `ARCHITECTURE.md`.
   Their seam rules belong in `HYDROLOGY.md`.

Seams are tested in G3.

---

## 12. Out of Scope

This document does **not** cover:

- Exact field list of a chunk base → `WORLDGEN.md`
- Modification file format → `SAVE_FORMAT.md`
- Streaming policy, load radius, eviction order → `STREAMING.md`
- LOD selection policy, blending → `RENDERING.md`
- Hydrology seam rules → `HYDROLOGY.md`
- Multi-threaded chunk generation → `DETERMINISM.md`

---

## 13. Open Questions

None. **This document is frozen.**