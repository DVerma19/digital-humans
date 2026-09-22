# Coordinates

> **Status:** Frozen baseline
> **Depends on:** `ARCHITECTURE.md` §4
> **Supersedes:** nothing

This document defines the four coordinate spaces used by Digital Humans,
the types that represent them, the only functions allowed to convert between
them, and the tolerances those functions must satisfy.

Nothing outside this document may reinterpret one space as another.

---

## 1. The Four Spaces

| Space | Purpose | Origin | Unit |
|---|---|---|---|
| **WORLD** | Authoritative simulation truth | World frame origin | meter |
| **CHUNK** | Integer chunk address + local offset | Chunk corner | meter |
| **RENDER** | GPU-ready, camera-relative | Camera position | meter |
| **SCREEN** | Swapchain pixels | Top-left of image | pixel |

Every crossing between spaces must go through a function in §5.

---

## 2. Axes

- Right-handed coordinate system.
- **Y is up.**
- `+X` = east
- `+Y` = up
- `+Z` = north

Sea level is `Y = 0`.

Terrain vertical range is defined by the generation profile, not hard-coded here.

---

## 3. Types

| Type | Fields | Notes |
|---|---|---|
| `WorldPos` | `f64 x, y, z` | Meters. Authoritative. Never converted to f32 except in `world_to_render`. |
| `ChunkAddress` | `i32 x, z` | Chunk grid coordinates. 2D for terrain; 3D only if underground is added later. |
| `LocalCoord` | `f32 x, y, z` | Meters within a chunk. `x` and `z` in `[0, CHUNK_SIZE_XZ)`. |
| `RenderPos` | `f32 x, y, z` | Meters relative to camera. |
| `ScreenPos` | `f32 x, y, depth` | Pixels plus NDC depth. |

`WorldPos` uses `f64`. Everything downstream may use `f32` **only after
subtracting the camera position in `f64` first**.

---

## 4. Constants

All values live in one header. They are not scattered across the codebase.

| Constant | Value | Meaning |
|---|---|---|
| `CHUNK_SIZE_XZ` | `64` | Meters per chunk edge, horizontal |
| `WORLD_SIZE_X` | `8192` | Meters, laboratory dataset width |
| `WORLD_SIZE_Z` | `5120` | Meters, laboratory dataset depth |
| `CHUNK_COUNT_X` | `128` | `WORLD_SIZE_X / CHUNK_SIZE_XZ` |
| `CHUNK_COUNT_Z` | `80` | `WORLD_SIZE_Z / CHUNK_SIZE_XZ` |

These are the **v1 world frame values**.

---

## 5. Conversion Functions

All functions below are **free functions** in namespace `dh::coords`.

No methods. No overloads that hide a space change.

```cpp
ChunkAddress world_to_chunk(WorldPos p);
LocalCoord   world_to_local(WorldPos p);
WorldPos     chunk_local_to_world(ChunkAddress c, LocalCoord l);

RenderPos    world_to_render(WorldPos p, WorldPos camera);
WorldPos     render_to_world(RenderPos p, WorldPos camera);

ScreenPos    world_to_screen(WorldPos p, Camera cam, Viewport vp);
Ray          screen_to_world_ray(ScreenPos p, Camera cam, Viewport vp);
```

### Rules

1. `world_to_chunk` uses **floor division**, not truncation.
   Negative coordinates must map to the correct chunk.
2. `world_to_local` returns `x, z` in `[0, CHUNK_SIZE_XZ)`. Never negative.
3. `world_to_render` is the **only** place `f64` becomes `f32`.
   It subtracts the camera position in `f64` first, then casts.
4. `screen_to_world_ray` is the inverse of projection.
   It returns a **ray**, not a point, because depth is ambiguous.

---

## 6. Tolerance

These numbers are the contract. Gates **G1**, **G3**, and **G4** test exactly these.

| Round trip | Tolerance |
|---|---|
| `world → chunk → world` (integers) | exact |
| `world → chunk → world` (floats) | `1e-9 * CHUNK_SIZE_XZ` |
| `world → render → world` | 1 mm up to 1 km distance |
| `world → screen → world ray` | 1 pixel |
| Chunk boundary sample (adjacent chunks) | `1e-6` world units |

---

## 7. Ownership

| Owner | Space | Rule |
|---|---|---|
| Simulation | **WORLD** | Nothing else writes it. |
| Chunk manager | **CHUNK** | Pure function of WORLD. |
| Renderer | **RENDER** | Pure function of WORLD + camera. |
| Swapchain | **SCREEN** | Pure function of RENDER + camera + viewport. |

**No reverse flow.** The renderer never writes WORLD.

---

## 8. Out of Scope

This document does **not** cover:

- Camera math → belongs in `CAMERA.md`
- Projection matrices → belongs in `RENDERING.md`
- Chunk halo rules → belongs in `CHUNKS.md`
- Precision strategy beyond the camera subtraction → belongs in `PERFORMANCE.md`

This document defines only the spaces, types, conversions, and tolerances.

---

## 9. Open Questions

None. **This document is frozen.**