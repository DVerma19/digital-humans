# Digital Humans

## Procedural World and Civilization Architecture

**Architecture baseline:** v0.1  
**Date:** 22 September 2026  
**Status:** Legacy planning baseline; superseded by [`ARCHITECTURE.md`](ARCHITECTURE.md)

> This document preserves the original architecture plan in readable Markdown form. New implementation decisions should be recorded against the current architecture document and its acceptance gates.

## 1. Purpose and Working Rule

The project needs an explicit world-generation and civilization architecture before further implementation. Earlier visual failures showed that world coordinates, rendering coordinates, source resolution, and camera behavior had become coupled.

The development loop is:

```text
DESIGN -> IMPLEMENT ONE SYSTEM -> RUN -> OBSERVE
       -> USER TEST -> AUTOMATED TEST -> FREEZE -> NEXT SYSTEM
```

Only one subsystem changes at a time. A visual improvement is not accepted merely because a screenshot looks better; it must preserve all previously passing contracts. Every stage requires a deterministic test scene and a user-facing inspection mode.

### Target interaction principle

```text
ZOOM OUT: SEE THE WORLD
ZOOM IN:  SEE A REGION
ZOOM IN:  SEE A SETTLEMENT
CLICK:    SEE A PERSON
CLICK:    SEE A MIND
TRACE:    SEE A LIFE
REWIND:   SEE CAUSAL CHANGE
```

## 2. Engineering Principles

The goal is not to copy Minecraft. The useful principles are on-demand generation, deterministic chunking, layered procedural fields, lower-resolution simulation with higher-resolution presentation, and camera-relative rendering.

| Principle | Digital Humans interpretation |
| --- | --- |
| Determinism | Seed, world coordinate, and generation version produce the same authoritative result. |
| Chunking | Simulation, visual geometry, and persistence operate on spatial chunks. |
| On-demand generation | Generate only the detail required by the camera, simulation, and interaction. |
| Layered fields | Use semantic macro, regional, and local fields instead of one generic noise function. |
| Interpolation | Keep simulation resolution separate from render resolution. |
| Local coordinates | Keep large deterministic world coordinates separate from camera-relative GPU coordinates. |
| Base plus modifications | Store persistent changes separately from procedural generation. |

## 3. System Architecture

```text
REAL-WORLD DATA / CALIBRATION
            |
            v
POPULATION + ENVIRONMENT INITIAL CONDITIONS
            |
            v
AUTHORITATIVE WORLD MODEL
  geology -> climate -> hydrology -> ecology -> resources
            |
            v
CIVILIZATION SIMULATION
  people -> households -> groups -> settlements -> institutions -> networks
            |
            +----------------------+----------------------+
            |                                             |
            v                                             v
PERSISTENCE / REPLAY                         VISUALIZATION
                                              chunks -> LOD -> GPU -> UI
```

### Ownership rules

- Simulation state is authoritative. The renderer reads it; the renderer does not define it.
- World generation is deterministic and versioned. A generator change creates a new generation version.
- World, chunk, render, and screen coordinates are distinct spaces.
- A simulation cell is not a render polygon. Several visual levels may represent the same data.
- Derived visual detail must never change authoritative state unless an explicit simulation system permits it.
- Persistent modifications are stored separately from procedural base generation.

## 4. Procedural World Hierarchy

The existing `8192 x 5120` world and `256 x 160` macro grid are laboratory data, not a limit on visual detail.

| Level | Purpose | Examples | Generation rule |
| --- | --- | --- | --- |
| L0 Global | Whole-world constraints | Bounds, seed, ocean ratio, climate zones | Once from seed and profile |
| L1 Macro | Continental structure | Elevation, temperature, rainfall, land mask, drainage basins | Stable low-frequency fields |
| L2 Regional | Natural landscape structure | Ridges, valleys, lakes, rivers, biomes, resources | Chunk-addressable from L1 and local functions |
| L3 Local | Renderable and interactive detail | Surface variation, vegetation, materials, small features | Active or visible chunks only |
| L4 Simulation | Human-scale state | People, households, buildings, roads, events, relationships | Scheduled and spatially indexed |

### Coordinate contract

| Space | Meaning |
| --- | --- |
| **World** | Authoritative, stable, deterministic coordinates |
| **Chunk** | Integer chunk address plus local coordinates |
| **Render** | Camera-relative GPU coordinates |
| **Screen** | Swapchain pixel coordinates |

Every conversion must be explicit, named, and tested. No subsystem may silently reinterpret one space as another.

## 5. World Generation Pipeline

```text
SEED + GENERATION PROFILE
  -> WORLD FRAME
  -> GEOLOGY / BASE ELEVATION
  -> CLIMATE FIELDS
  -> HYDROLOGY GRAPH
  -> TERRAIN SHAPING / EROSION
  -> BIOMES / ECOLOGY
  -> RESOURCES
  -> ACCESSIBILITY / SETTLEMENT SUITABILITY
  -> INITIAL INFRASTRUCTURE SEEDS
  -> CIVILIZATION INITIAL CONDITIONS
```

### 5.1 World frame

Define dimensions, origin, wrapping rules, sea level, vertical convention, calendar, generation version, chunk addressing, cache keys, and serialization versions. Store seed, generator version, simulation ruleset version, and calibration profile in world metadata.

### 5.2 Geology and terrain

Base elevation must use independent semantic fields: continental mass, land mask, broad elevation, mountain propensity, basin shaping, regional roughness, and local detail. Controlled domain warping may be added when it improves coherence.

### 5.3 Climate

- Temperature depends on latitude-equivalent position, elevation, and broad circulation.
- Rainfall depends on circulation, proximity to water, elevation barriers, and regional moisture.
- Seasonality remains a separate layer where possible.
- Climate fields remain continuous so they can drive agriculture, disease, vegetation, and migration.

### 5.4 Hydrology

Drainage is an authoritative graph, not a painted river mask. Track flow direction, accumulation, catchment, river order, width, water availability, lake identity, and basin membership. Use hydrology to shape terrain and preserve freshwater and transport queries for settlements.

### 5.5 Biomes and ecology

Biomes derive from environmental gradients. Transition zones may use weighted suitability. Vegetation and wildlife remain separate layers so ecological state can change without redefining biome identity.

### 5.6 Resources

Represent resources as fields and, where needed, as extractable or renewable stocks. Keep food potential, biomass, fertility, timber, stone, minerals, wildlife, and freshwater distinct. A resource field is not the same thing as an owned resource.

## 6. Chunks, Streaming, Persistence, and LOD

| Subsystem | Required behavior | Failure to avoid |
| --- | --- | --- |
| Generator | Generate from a deterministic key and neighboring context | Approach direction changes the result |
| Streamer | Load active camera and simulation regions on demand | Generate the whole world before interaction |
| Cache | Version keys and store modified state | Interpret stale data under a new generator |
| LOD | Change representation by distance and interaction importance | Render every object at maximum detail |
| Boundaries | Preserve seamless values across chunk edges | Seams, broken rivers, mismatched terrain |
| Local rendering | Translate active chunks relative to the camera | GPU precision errors at large coordinates |

The first visual milestone is to prove chunk coordinates, camera transforms, deterministic generation, seam continuity, and LOD boundaries independently.

## 7. Rendering Architecture

```text
AUTHORITATIVE WORLD DATA
  -> SPATIAL CHUNK MANAGER
  -> VISUAL CHUNK BUILDER
  -> LOD SELECTION
  -> MATERIAL / TERRAIN PASSES
  -> GPU BUFFERS / TEXTURES
  -> VULKAN RENDER GRAPH
  -> SCREEN
```

### Visual layers

1. **Macro map:** land, water, climate, and biomes.
2. **Regional map:** relief, coastlines, rivers, lakes, vegetation, and routes.
3. **Local terrain:** surface detail, materials, vegetation, and structures.
4. **Entity layer:** people, animals, buildings, roads, and markers.
5. **UI layer:** profiles, relationships, cognition, history, and causal trace.

A close zoom must not expose raw simulation cells as giant rectangles unless a diagnostic layer is explicitly enabled.

## 8. Civilization Simulation

```text
ENVIRONMENT -> NEEDS / OPPORTUNITIES -> INDIVIDUAL DECISIONS -> ACTIONS
-> INTERACTIONS -> RELATIONSHIPS / HOUSEHOLDS -> GROUPS / SETTLEMENTS
-> EXCHANGE / SPECIALIZATION -> NORMS / CUSTOMS
-> INSTITUTIONS / INFRASTRUCTURE -> CIVILIZATION-LEVEL PATTERNS
-> ENVIRONMENTAL FEEDBACK
```

### 8.1 Person model

| Domain | Core state |
| --- | --- |
| Identity | Stable ID, name, birth time, age, sex, household, residence |
| Body and health | Health, energy, nutrition, age effects, disease, injury, mortality inputs |
| Location | World position, movement, building, settlement, region |
| Skills and work | Skills, experience, occupation, workplace, productivity |
| Needs | Food, water, shelter, safety, social contact, rest |
| Resources | Wealth, inventory, assets, claims, and access rights |
| Mind | Goals, beliefs, concepts, values, preferences, fears, and memories |
| Social | Relationships, family, groups, reputation, and trust |
| Decision state | Current intention, alternatives, selected action, and reasons |
| History | Events, memories, relationships, moves, jobs, and life events |

### 8.2 Cognitive model

The first brain is interpretable, deterministic, serializable, and does not require an LLM per person.

```text
EVENT -> PERCEPTION / INTERPRETATION
      -> IMPORTANCE + EMOTION
      -> MEMORY
      -> BELIEF / ASSOCIATION UPDATE
      -> DECISION BIAS / GOAL UPDATE
      -> ACTION
      -> NEW EVENT
```

Memory decays, reinforces, and associates. Beliefs record provenance. Decisions expose the features that influenced the selected action. The cognitive graph can be serialized and replayed for a selected person.

### 8.3 Social structure

Relationships evolve from interactions. Households are first-class entities because food, shelter, income, child care, and inheritance often operate at household scale. Groups and institutions emerge from repeated interaction, shared interests, coordination needs, and accumulated norms.

### 8.4 Economy and production

The economy requires needs, production, exchange, prices or scarcity, ownership, transport, and accumulation. Inputs, skills, time, tools, outputs, locations, and accounting must remain inspectable.

### 8.5 Emergent civilization

- Settlements form where suitability, water, food, transport, and clustering support residence.
- Roads and trade routes emerge from repeated movement and economic value.
- Customs and norms emerge from repeated behavior and social reinforcement.
- Institutions emerge when coordination problems persist.
- Technology is knowledge plus social transmission plus material prerequisites.
- Civilization feeds back into the environment through land use, extraction, construction, and population density.

## 9. Real-World Calibration

Real-world data defines distributions and calibration targets, never individual identities.

| Target | Data family | Use |
| --- | --- | --- |
| Population, age, sex | [UN WPP 2024](https://population.un.org/wpp/downloads/) | Demographics, mortality, fertility |
| Census microstructure | [IPUMS International](https://www.ipums.org/projects/ipums-international/d020.V7.6) | Household, education, and work structure |
| Migration | UN and [OWID](https://ourworldindata.org/population-sources) | Movement rates and origin-destination constraints |
| Socioeconomic indicators | World Bank, OWID, national statistics | Income, labor, education, development |

Choose the reference population, period, geography, and licensing before building a data-driven population generator. Do not mix historical and current data without an explicit time model.

## 10. Time and Scheduling

Use multiple time scales with deterministic ordering:

| Cadence | Typical systems |
| --- | --- |
| Seconds or sub-second | Movement and local interactions |
| Minutes or hours | Travel, consumption, work, and production |
| Days | Household consumption, transactions, births, deaths, routine social activity |
| Weeks or months | Employment, migration, settlement growth, relationship drift |
| Years | Aging, demographic change, institutions, technology, and culture |

Every event receives a deterministic timestamp and stable tie-breaker.

## 11. Persistence, Events, and Replay

Every major change needs a causal trail.

| Record | Required fields |
| --- | --- |
| Event | ID, timestamp, type, people, location, cause, effects, importance, memories |
| Person snapshot | Stable ID and dynamic state needed to reconstruct a point in time |
| Chunk state | Procedural base key, version, and explicit modifications |
| Relationship edge | Endpoints, type, strength, provenance, and active period |
| Institution | Members, roles, rules, resources, scope, and history |
| Replay checkpoint | Seed, versions, time, state hash, and serialized state pointer |

Replay must reproduce the same selected life path from a checkpoint and event sequence. State hashes must be computed from canonical authoritative state.

## 12. Acceptance Gates

| Gate | System | Automated acceptance |
| --- | --- | --- |
| G0 | Build baseline | Build succeeds and executable launches |
| G1 | Coordinates | World, chunk, and render transforms round-trip within tolerance |
| G2 | Deterministic generation | Authoritative chunk hashes match across runs |
| G3 | Chunk seams | Adjacent border samples agree |
| G4 | Camera | Center, pan, zoom, cursor zoom, and edges are correct |
| G5 | LOD | Transitions remain within the defined visual tolerance |
| G6 | Hydrology | Flow graph is valid and reproducible |
| G7 | Biomes and ecology | Classification matches input fields |
| G8 | Resources | Extraction changes only the intended state |
| G9 | Ten-person population | Population invariants hold on every tick |
| G10 | Relationships and memory | Major changes have event provenance |
| G11 | Household and economy | Accounting balances and transactions replay |
| G12 | Settlement | Formation is deterministic and spatially consistent |
| G13 | Replay | State hash and selected event sequence match |
| G14 | Scale | 10 to 10,000 people stay within measured budgets |

## 13. Diagnostic Views

- Elevation, slope, and normal
- Land, ocean, and shoreline
- Temperature, rainfall, and season
- Watersheds, flow direction, river order, and lakes
- Biome suitability and transitions
- Resource fields
- Chunk boundaries, IDs, and load state
- LOD level
- Population density and settlement suitability
- Human movement paths and current tasks
- Relationships and cognitive graph around a selected person
- Event timeline and causal chain
- Simulation time, tick rate, queues, entities, and performance counters

## 14. Implementation Order

1. Freeze the architecture and test harness.
2. Prove coordinate and camera contracts.
3. Build the deterministic chunk generator.
4. Import the existing world as a macro field.
5. Add regional procedural detail.
6. Build the authoritative hydrology graph.
7. Add LOD and streaming.
8. Add materials, ecology, and resources.
9. Compute settlement suitability.
10. Generate a calibrated ten-person population.
11. Implement needs, movement, and actions.
12. Add relationships, memory, and cognition.
13. Add households, production, and exchange.
14. Add settlement emergence.
15. Add institutions, culture, and technology.
16. Add replay and causal tracing.
17. Run scale tests.

## 15. Vulkan Work

The current Vulkan path remains the renderer implementation target. The existing world GPU binary may transport macro fields, but it is not the final terrain representation.

- Freeze the current working build as a baseline.
- Do not change shaders to compensate for camera or data-resolution problems.
- Keep the main application stable while lower-level contracts are established.
- Add diagnostic render modes before visual polish.
- Build coordinate and chunk contracts before richer materials or assets.

## 16. Anti-Patterns

| Anti-pattern | Why it fails |
| --- | --- |
| One giant noise shader as the world model | Produces a picture rather than a queryable world |
| Render resolution equals simulation resolution | Close zoom exposes cells and forces expensive regeneration |
| Rivers drawn after terrain | Rivers become cosmetic instead of authoritative infrastructure |
| Hard-coded people | Prevents scale and statistical calibration |
| LLM per person from day one | Expensive, opaque, and difficult to debug |
| Renderer owns truth | Visual bugs can become simulation bugs |
| Multiple systems changed together | Failures cannot be attributed |
| No hashes or replay | Emergent behavior cannot be reproduced |
| No generation version | Parameter changes silently mutate existing worlds |

## 17. Definition of Done

- Coordinate spaces are explicit and tested.
- A seed and generation profile reproduce world data.
- Generation is hierarchical and chunk-addressable.
- Hydrology is authoritative and continuous across chunks.
- Rendering has independent LODs and does not expose raw macro cells at arbitrary zoom.
- Simulation owns truth, events, persistence, and replay.
- A ten-person synthetic population comes from distributions rather than hand-authored individuals.
- Each subsystem has a diagnostic view and deterministic acceptance test.
- Selected histories can be replayed.

## 18. Sources

- [Notch, “Terrain generation, Part 1”](https://blog.omniarchive.uk/post/3746989361/terrain-generation-part-1/)
- [Joshua Tippetts, “Creator of worlds”](https://media.gdcvault.com/GD_Mag_Archives/GDM_April_2011.pdf)
- [Genevaux et al., “Terrain generation using procedural models based on hydrology”](https://doi.org/10.1145/2461912.2461996)
- [Peytavie et al., “Procedural Riverscapes”](https://onlinelibrary.wiley.com/doi/10.1111/cgf.13814)
- [Grenier et al., “Real-time Terrain Enhancement with Controlled Procedural Patterns”](https://onlinelibrary.wiley.com/doi/10.1111/cgf.14992)

---

**Baseline status:** Architecture planning complete. Implementation remains paused until the first isolated acceptance gate passes.
