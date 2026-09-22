# Hydrology Parameters

> **Status:** Frozen baseline v1
> **Depends on:** `HYDROLOGY.md`
> **Supersedes:** nothing

Values used by `hydro.cpp`. These are parameters, not contract.

## Sea level

    SEA_LEVEL_METERS = 0.0

A cell is ocean if its elevation is strictly below sea level.

## Basin grid

    BASIN_W = 128   (== CHUNK_COUNT_X)
    BASIN_H = 80    (== CHUNK_COUNT_Z)

One basin cell per chunk. Elevation sampled at chunk center (32, 32).

## D8 directions

    DIR_NONE = 255
    0 = E  ( 1,  0)
    1 = SE ( 1,  1)
    2 = S  ( 0,  1)
    3 = SW (-1,  1)
    4 = W  (-1,  0)
    5 = NW (-1, -1)
    6 = N  ( 0, -1)
    7 = NE ( 1, -1)

## Depression filling

Before computing flow, depressions are filled by priority-flood:

    1. Seed a min-priority queue with every ocean cell and every grid-boundary cell.
       Set filled_elev[cell] = original_elev[cell].
    2. Pop the cell C with the lowest filled_elev.
    3. For each unresolved neighbor N:
           filled_elev[N] = max(original_elev[N], filled_elev[C])
           record discovered_by[N] = C
           push N onto the queue.
    4. Repeat until the queue is empty.

After this, every non-outlet cell has a well-defined downhill path to an outlet.
The path is C -> discovered_by[C] -> discovered_by[discovered_by[C]] -> ... -> outlet.

## Flow rule

    direction[cell]      = direction toward discovered_by[cell]
    outlet cells (ocean and grid boundary) have direction = DIR_NONE.
    is_lake[cell]        = filled_elev[cell] > original_elev[cell] + epsilon
    lake_depth[cell]     = filled_elev[cell] - original_elev[cell]

## Accumulation

    accum[cell] starts at 1.
    Processed in descending elevation order.
    Each cell adds its accum to its downstream target.

## Basin IDs

    Ocean cells: unique ID starting at 1.
    Endorheic / lake: 0.
    Land with flow: inherits downstream basin ID.
    Processed in ascending elevation order.

## Strahler order

    No incoming flow: order = 1.
    One incoming, or many with equal max order < 2: order = max.
    Two or more incoming of same max order: order = max + 1.
    Capped at 12.


## Pass 2 refinement (per chunk)

Local D8 flow on a 1-cell halo elevation grid. Interior cells only.

    RIVER_MIN_ACCUM   = 4.0   (below this, no river)
    RIVER_FULL_ACCUM  = 40.0  (above this, river is fully saturated)
    LAKE_EPSILON      = 0.05  (meters)

Per-cell output:

    river_flow   = clamp((local_accum - RIVER_MIN_ACCUM)
                       / (RIVER_FULL_ACCUM - RIVER_MIN_ACCUM), 0, 1)
    lake_depth   = filled_elevation - elevation  if basin cell is a lake
                 = 0 otherwise
    Both are 0 in ocean cells.

Out-of-world chunks (address outside BASIN grid): all water = 0.

## Open questions

Parameters may be tuned after the first dump. Any tune updates this file.