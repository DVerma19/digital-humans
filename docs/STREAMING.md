# Streaming

> **Status:** Frozen baseline
> **Depends on:** `CHUNKS.md`, `COORDINATES.md`
> **Supersedes:** nothing

This document defines how chunks are loaded and unloaded around the
camera. It is the bridge between the persistent chunk contract and the
renderer.

Nothing here defines LOD, blending, or prioritization beyond "nearest
first". Those belong in `RENDERING.md` once LOD exists.

---

## 1. Streaming Center

The streaming center is the **chunk containing the camera position**.

```
center = world_to_chunk(camera_world_position)
```

No other center is permitted in v1. As the camera moves, the center
updates when the camera crosses a chunk boundary.

---

## 2. Radii

| Radius | Meaning |
|---|---|
| `load_radius` | Chebyshev distance from center within which chunks must be loaded |
| `evict_radius` | Chebyshev distance beyond which a loaded chunk is removed |

In v1: `evict_radius = load_radius + 2`. This hysteresis prevents thrashing
when the camera jitters across a chunk boundary.

Both radii are integers in chunk units.

---

## 3. Determinism

Streaming does **not** affect chunk content. The same chunk address
produces the same bytes whether it was streamed in from the west or the
east, first or last.

Streaming affects only **what is present in memory**, never what a chunk
contains.

---

## 4. Per-Frame Budget

The streamer builds at most `max_builds` chunks per frame. This bounds
frame time regardless of how many chunks are pending.

Any chunks not built this frame remain queued and are built on later
frames.

There is no hard limit on total loaded chunks. Memory use is bounded by
`load_radius` and chunk mesh size.

---

## 5. Build Order

Pending chunks are built nearest-first, measured by Chebyshev distance
to the center. Ties break by `(x, z)` ascending. This guarantees
deterministic loading order across runs.

---

## 6. Eviction

Eviction happens **before** building in the same update call. This frees
memory for incoming chunks.

Eviction does not need to preserve determinism across frames — the
loaded set is a function of camera position and load/evict radii, not
of history.

A chunk may be evicted and later re-streamed. That is legal and does
not invalidate anything.

---

## 7. Ownership

The streamer owns the GPU resources of every loaded chunk. It destroys
them on eviction and on shutdown.

The renderer reads the current loaded set each frame. It does not
retain references across frames.

---

## 8. Out of Scope

- LOD selection → `RENDERING.md`
- Blending between LODs → `RENDERING.md`
- Async / threaded building → `SCHEDULER.md` (later)
- Persistence of GPU buffers → not persistent; rebuilt on demand
- Camera-relative vertex coordinates → `PERFORMANCE.md` (later)

---

## 9. Open Questions

None. **This document is frozen.**