# Digital Humans — Procedural World + Civilization Architecture

## Architecture Baseline v0.2 (Revised)

**Date:** 22 September 2026
**Status:** Supersedes v0.1. This revision adds the end-goal definition, the uniqueness clause, the cognition data model, the determinism contract, the hydrology boundary algorithm, the hardware budget, the build/dependency policy, the save format, and the Phase A / Phase B split.

---

## 0. Changelog from v0.1

| Section | Change |
|---|---|
| §1 | Added **End Goal** and **Definition of Success** |
| §1.1 | **NEW** — Uniqueness Clause (what makes this not a clone) |
| §4.1 | Coordinate contract — now with exact types and tolerances |
| §5.4 | Hydrology — now with the two-pass boundary algorithm |
| §6.1 | **NEW** — Chunk contract and halo rules |
| §6.2 | **NEW** — Save format and versioning |
| §8.2 | Cognition — now with a concrete data model |
| §10.1 | **NEW** — Multi-rate scheduler table |
| §11.1 | **NEW** — Event log, hash, and replay format |
| §11.2 | **NEW** — Determinism contract for parallel workers |
| §12.1 | **NEW** — Build, dependencies, CI (zero cost) |
| §12.2 | **NEW** — Hardware budget for target machine |
| §15 | Scale targets now with numbers |
| §16 | Implementation order now split into **Phase A** and **Phase B** |
| §18 | Anti-patterns expanded |
| §21 | **NEW** — Definition of Done for each phase |
| §22 | **NEW** — Decision log requirements |

---

## 1. Purpose, End Goal, and Definition of Success

### 1.1 The End Goal

**Build a deterministic, inspectable, procedurally generated world where simulated people live real simulated lives, and the user can zoom from the whole planet into a single mind and rewind the causal chain of any event.**

The program is not a game. It has no win condition, no score, no player character. It is an **interactive documentary** — a world you observe, inspect, and trace.

### 1.2 What Success Looks Like

The project is successful when **all** of the following are true:

1. A seed and a generation profile produce the same world, the same people, and the same history, every time, on any machine, in any thread configuration.
2. A user can zoom continuously from planetary view to a single person without the world breaking, popping, or exposing raw simulation cells.
3. A user can click a person and see their needs, memories, beliefs, relationships, and current decision — with the reason for that decision visible.
4. A user can trace any event backwards through its causes to an originating condition.
5. A user can rewind simulation time and verify that a previously observed life path replays identically.
6. A population of at least 10,000 agents runs at a stable tick rate on the target hardware (§12.2) without architectural rewrite.
7. Every subsystem has a diagnostic view and a deterministic acceptance test.

### 1.3 The Uniqueness Clause

This project is unique in the following specific, non-negotiable ways. If any of these are dropped, the project becomes a clone of something that already exists.

| Unique property | What it means | Why it is not a clone |
|---|---|---|
| **Continuous zoom** | Planet → region → settlement → person → mind, in one uninterrupted camera motion | No existing game does this across all scales |
| **Rewindable causal trace** | Every event has a queryable chain of causes going back to origin | No existing game does this at agent scale |
| **Deterministic replay of lives** | A selected person's life can be replayed bit-exactly from a seed | No existing game does this |
| **Authoritative hydrology** | Rivers are queryable infrastructure, not painted overlays | Rare in games, standard in research |
| **Calibrated synthetic population** | Demographics derived from real statistical distributions, not hand-authored | Standard in academia, absent in games |
| **Rule-based cognition with provenance** | Beliefs and decisions carry the memory that produced them | No LLM required; fully inspectable |
| **Multi-scale time** | Seconds to years on independent cadences, with deterministic ordering | Rare in games, necessary at this scale |

**The combination is the point.** Every system exists to serve the causal trace. If a feature does not serve the trace, it is decorative.

### 1.4 Non-negotiable development loop

```
DESIGN → SPECIFY → IMPLEMENT ONE SYSTEM → RUN → OBSERVE
→ USER TEST → AUTOMATED TEST → FREEZE → NEXT SYSTEM
```

A system is not frozen until its spec is in `docs/`, its tests pass, and its state hash is recorded. No exceptions.

---

## 2. Inherited Principles from Notch / Minecraft

The goal is not to copy Minecraft. The engineering principles are:

| Principle | Digital Humans interpretation |
|---|---|
| Determinism | Seed + world coordinate + generation version = same authoritative result |
| Chunking | Simulation, geometry, and persistence all operate on spatial chunks |
| On-demand generation | Generate only what camera, simulation, and interaction require |
| Layered noise | Independent semantic fields, not one generic FBM |
| Local coordinates | Render in camera-relative space; simulation in world space |
| Base + modifications | Persistent changes stored separately from procedural base |

---

## 3. Core Concepts and Terminology

| Term | Meaning |
|---|---|
| **Authoritative state** | Simulation-owned truth. Only simulation systems may mutate it. |
| **Derived state** | Computed from authoritative state. Never written back without an explicit system. |
| **Chunk** | Fixed-size spatial unit for generation, streaming, and persistence. |
| **Halo** | Neighbor data required by a chunk to compute boundary-correct results. |
| **Generation profile** | Parameter set that, with a seed, produces a world. Versioned. |
| **Ruleset** | Parameter set that governs simulation behavior. Versioned. |
| **Event** | A recorded occurrence with a timestamp, cause, participants, and effects. |
| **Trace** | The causal chain from an event backwards to its originating conditions. |
| **State hash** | A canonical digest of authoritative state at a given tick. |
| **Gate** | An acceptance test that must pass before the next system may start. |

---

## 4. Coordinate Contract

### 4.1 Spaces

| Space | Definition | Type |
|---|---|---|
| **WORLD** | Authoritative simulation coordinates. Stable, deterministic. Origin = world frame origin. | `double` (f64) for position, `int64` for chunk address |
| **CHUNK** | Integer chunk address + local coordinates within the chunk. | `ChunkAddress { int32 x, y, z }`, `LocalCoord { float x, y, z }` |
| **RENDER** | Camera-relative GPU coordinates. Recomputed each frame. | `float` (f32), origin = camera |
| **SCREEN** | Swapchain pixels. | `int32` |

### 4.2 Conversion rules

- Every conversion function is explicit, named, and tested.
- No subsystem may silently reinterpret one space as another.
- Render coordinates are always derived from world coordinates relative to the camera, never the reverse.
- Chunk address = `floor(world_pos / chunk_size)` using `std::floor`, not truncation.

### 4.3 Tolerance

- World → Chunk → World round-trip: exact for integer positions, within `1e-6` of chunk size for floats.
- World → Render → Screen → World: within 1 pixel of the original screen position.
- Tests in G1 and G4 enforce these.

---

## 5. Procedural World Hierarchy

| Level | Purpose | Generation rule |
|---|---|---|
| **L0 Global** | World bounds, seed, ocean ratio, climate envelopes | Once from seed + profile |
| **L1 Macro** | Continents, elevation, temperature, rainfall, ocean/land, major basins | Deterministic low-frequency fields |
| **L2 Regional** | Ridges, valleys, lakes, rivers, biome transitions, resources | Chunk-addressable; uses L1 + local functions |
| **L3 Local** | Surface variation, vegetation, materials, small features | Active/visible chunks only; may exceed sim resolution |
| **L4 Simulation** | People, households, buildings, roads, events, relationships | Scheduled by simulation time; spatially indexed |

---

## 6. World Generation Pipeline

```
SEED + GENERATION PROFILE
→ 1. WORLD FRAME
→ 2. GEOLOGY / BASE ELEVATION
→ 3. CLIMATE FIELDS
→ 4. HYDROLOGY GRAPH
→ 5. TERRAIN SHAPING / EROSION
→ 6. BIOMES / ECOLOGY
→ 7. RESOURCES
→ 8. ACCESSIBILITY / SETTLEMENT SUITABILITY
→ 9. INITIAL INFRASTRUCTURE SEEDS
→ 10. CIVILIZATION INITIAL CONDITIONS
```

### 6.1 Hydrology boundary algorithm (NEW)

Hydrology is global but generation is chunk-local. Resolve this with a **two-pass** approach:

**Pass 1 — Global drainage (L1 resolution, one time per world):**
- Compute a coarse drainage grid (e.g., 512 × 320 cells for a 8192 × 5120 world).
- Flow direction, accumulation, basin ID, and river order are computed here.
- Stored as a world-level artifact. Never recomputed per chunk.

**Pass 2 — Chunk refinement (L2/L3, per chunk):**
- Each chunk receives its macro basin context as input.
- Local flow refines within the basin but cannot change basin membership.
- River width and order are derived from the global upstream accumulation, sampled at the chunk boundary.

**Seam guarantee:** two adjacent chunks see the same macro basin field, so their boundary river segments agree by construction. Tested in G6.

### 6.2 Chunk contract (NEW)

| Property | Value |
|---|---|
| Chunk size (horizontal) | 64 × 64 cells |
| Chunk size (vertical) | Column-based; terrain height derived from L1/L2 |
| Halo | 1 cell in each horizontal direction for L2 boundary correctness |
| Addressing | `ChunkAddress { int32 x, z }` (2D for terrain; 3D if underground added later) |
| Cache key | `(world_seed, generation_version, chunk_address, lod_level)` |
| Persistence | Base procedural key + explicit modification diff |
| Versioning | Generator version recorded in every chunk header |

### 6.3 Save format (NEW)

| Artifact | Format | Version |
|---|---|---|
| World metadata | Binary, fixed-layout header + JSON profile | `WMF v1` |
| Chunk base | Procedural, not stored | — |
| Chunk modifications | Binary diff, sparse | `CHD v1` |
| Event log | Append-only binary, fixed record size | `EVL v1` |
| Person snapshot | Binary, canonical order | `PSN v1` |
| Relationship edges | Binary, canonical order | `REL v1` |
| Institution records | Binary, canonical order | `INS v1` |
| Replay checkpoint | Binary blob + state hash | `RCP v1` |

All formats little-endian, no padding, canonical field order. Hash = BLAKE3 over canonical bytes.

---

## 7. Rendering Architecture

```
AUTHORITATIVE WORLD DATA
→ spatial chunk manager
→ visual chunk builder
→ LOD selection
→ material / terrain passes
→ GPU buffers / textures
→ Vulkan render graph
→ screen
```

### 7.1 Visual layers

- **Macro map** — land, water, climate, biomes
- **Regional map** — relief, coastlines, rivers, lakes, vegetation, routes
- **Local terrain** — surface detail, materials, vegetation instances, structures
- **Entity layer** — people, animals, buildings, roads, markers
- **UI layer** — profile, relationships, cognitive graph, history, causal trace

### 7.2 Rendering rule

A close zoom must never reveal raw simulation cells as giant rectangles unless the user explicitly enables a diagnostic layer. Detail at close range is generated from the same authoritative world state.

---

## 8. Civilization Simulation Architecture

```
ENVIRONMENT → NEEDS / OPPORTUNITIES → INDIVIDUAL DECISIONS → ACTIONS
→ INTERACTIONS → RELATIONSHIPS / HOUSEHOLDS → GROUPS / SETTLEMENTS
→ EXCHANGE / SPECIALIZATION → NORMS / CUSTOMS
→ INSTITUTIONS / INFRASTRUCTURE → CIVILIZATION-LEVEL PATTERNS
→ ENVIRONMENTAL FEEDBACK
```

### 8.1 Person model

| Domain | Core state |
|---|---|
| Identity | Stable ID, name, birth time, age, sex, household, residence |
| Body / health | Health, energy, nutrition, age effects, disease/injury, mortality inputs |
| Location | World position, movement state, current building/settlement/region |
| Skills / work | Skills, experience, occupation, workplace, productivity |
| Needs | Food, water, shelter, safety, social contact, rest |
| Resources | Wealth, inventory, owned assets, claims |
| Mind | See §8.2 |
| Social | Relationships, family, group membership, reputation |
| Decision state | Current intention, alternatives, selected action, features used |
| History | Events experienced, memories, relationships changed, moves, jobs |

### 8.2 Cognition data model (NEW — critical)

The first brain is interpretable and rule-based. No LLM. Fully deterministic and serializable.

```
Person.mind:
  concepts   : ConceptGraph         // sparse
  memories   : MemoryStore          // ring buffer + importance index
  beliefs    : BeliefSet            // propositions with provenance
  values     : ValueWeights         // stable preferences
  goals      : GoalStack            // current + long-term
  emotion    : EmotionState         // valence, arousal
  decisions  : DecisionTraceRing    // last N decisions, for inspection
```

```
Concept {
  id           : ConceptId
  type         : ConceptType        // person | place | object | idea | action
  label        : string             // for UI inspection only
  valence      : f32                // -1..+1
  strength     : f32                // 0..1
}

Memory {
  id              : MemoryId
  event_ref       : EventId
  timestamp       : SimTime
  participants    : [EntityId]
  location        : SpatialHandle
  importance      : f32             // 0..1
  emotional_tag   : Emotion
  associations    : [ConceptId]
  decay_rate      : f32
  reinforcement   : u16
}

Belief {
  proposition  : (subject, predicate, object)
  confidence   : f32                // 0..1
  provenance   : [MemoryId] | [SocialSourceId]
  last_updated : SimTime
}

Goal {
  id         : GoalId
  type       : GoalType
  priority   : f32
  deadline   : SimTime (optional)
  parent     : GoalId (optional)
  status     : active | satisfied | abandoned
}

Decision {
  id            : DecisionId
  timestamp     : SimTime
  options       : [Option]
  selected      : OptionId
  features      : [FeatureContribution]   // what influenced the choice
  reason_text   : string                  // generated for UI
}
```

**Update cadences:**

| Process | Cadence |
|---|---|
| Needs decay | Every simulation tick |
| Movement | Every simulation tick |
| Decision re-evaluation | On event trigger, or every N seconds |
| Memory consolidation | Once per simulated day |
| Belief revision | On new evidence, or every simulated week |
| Goal revision | On major life event, or every simulated month |
| Value drift | Every simulated year |

**Decision function (first version):**
- Enumerate candidate actions from current goals and available affordances.
- Score each by weighted sum of: need satisfaction, goal alignment, expected cost, social consequence, risk.
- Select highest-scoring. Log feature contributions.
- Add small deterministic tie-breaker by `(entity_id, tick)`.

This is implementable, testable, and inspectable. It is the hardest single system in the project. It will need its own `docs/COGNITION.md`.

### 8.3 Social structure

- **Relationships**: parent, child, sibling, partner, friend, colleague, acquaintance, rival.
- **Strength and trust** evolve from interactions; never permanently assigned.
- **Households** are first-class entities (food, shelter, income, child care, inheritance).
- **Groups and institutions** emerge from repeated interaction, shared interest, and coordination needs.

### 8.4 Economy and production

| Layer | What must exist |
|---|---|
| Needs | Consumption requirements and urgency |
| Production | Inputs, skills, time, tools, output, location |
| Exchange | Offers, requests, transactions, or market mechanisms |
| Prices / scarcity | Local availability and demand |
| Ownership | Control of land, tools, inventories, buildings |
| Transport | Cost and time for moving people and goods |
| Accumulation | Savings, investment, inheritance, loss |

### 8.5 Emergent civilization systems

- Settlements form where suitability, water, food, transport, and social clustering make residence viable.
- Roads emerge from repeated movement and economic value.
- Customs arise from repeated behavior, social reinforcement, institutional memory.
- Institutions arise when coordination problems persist.
- Technology = knowledge + social transmission + material prerequisites.
- Civilization feeds back into environment through land use, extraction, construction, density.

**Status:** These are **research gates**. Success criteria may need revision. See §16 Phase B.

---

## 9. Real-World Calibration Without Copying Real Individuals

Real-world data defines distributions, constraints, and calibration targets — never identities.

| Variable | Data family | Use |
|---|---|---|
| Population size / age / sex | UN WPP 2024 | Initial demographics, mortality/fertility calibration |
| Census microstructure | IPUMS International | Household, education, work cross-tabs |
| Migration | UN / OWID | Movement rates, origin-destination constraints |
| Socioeconomic | World Bank / OWID / national | Income, labor, education calibration |

The exact reference population, period, geography, and licensing must be chosen before the first data-driven population generator. Historical and current data must not be mixed without an explicit time model.

---

## 10. Time and Simulation Scheduling

### 10.1 Multi-rate scheduler (NEW)

| Cadence | Systems | Budget at 10k agents |
|---|---|---|
| Sub-second (every tick, 20 Hz) | Movement, local interaction, rendering interpolation | 5 ms |
| Minutes / hours | Travel progress, local consumption, work actions, production | 5 ms |
| Days | Household consumption, economic transactions, births/deaths, routine social | 10 ms |
| Weeks / months | Employment change, migration decisions, settlement growth, relationship drift | 10 ms |
| Years | Aging, demographic restructuring, institutional change, long-term tech/culture | 5 ms |
| Reserve | Headroom | 15 ms |
| **Total tick budget** | | **50 ms (20 Hz)** |

Deterministic ordering is preserved across cadences: lower-cadence systems run at defined ticks and commit in the same ordered queue as higher-cadence systems.

---

## 11. Persistence, Events, Replay, Determinism

### 11.1 Event log, hash, and replay (NEW)

**Event record:**

```
Event {
  event_id        : u64
  timestamp       : SimTime
  event_type      : EventType
  people_involved : [PersonId]
  location        : SpatialHandle
  cause           : EventId | TriggerId
  effects         : [EffectRecord]
  importance      : f32
  related_memories: [MemoryId]
}
```

**Retention policy:**
- Recent events (last 1 simulated year): full detail, in memory.
- Older events: compressed, indexed by event_id and person_id.
- Checkpoints: full state hash + serialized state every simulated month.

**Hash:**
- BLAKE3 over canonical serialization of authoritative state.
- Computed at defined ticks (every simulated day, and before/after checkpoints).
- Used in every gate test.

**Replay:**
- Given seed + generation version + ruleset version + tick range, the simulation reproduces bit-exactly.
- Selected person replay: query event log by person_id, reconstruct from nearest checkpoint + events forward.

### 11.2 Determinism contract for parallel workers (NEW)

```
Commit order key = (tick, system_priority, entity_id, event_sequence)

Rules:
1. Workers compute in any order.
2. Workers submit results to a per-tick ordered commit queue.
3. The committer drains the queue in key order.
4. No system may read mutable state written by another system in the same tick.
5. Systems that must share state run in a defined order in the same worker.
6. RNG: one PCG64 stream per (system_id, tick), seeded by (world_seed, system_id, tick).
7. State hash is computed after commit, before the next tick.
8. Determinism test: same seed, 1 worker vs N workers, identical hash.
```

This contract is enforced by a dedicated test in G2 and G3.

---

## 12. Build, Hardware, and Cost

### 12.1 Build, dependencies, CI (NEW — zero cost)

| Concern | Choice | License | Cost |
|---|---|---|---|
| Language | C++23 | — | Free |
| Build | CMake + Ninja | BSD / Apache | Free |
| Compiler | GCC / Clang / MSVC | GPL / Apache / proprietary-free | Free |
| Window/input | SDL3 | zlib | Free |
| Graphics | Vulkan SDK | Apache | Free |
| Math | GLM | MIT | Free |
| ECS | EnTT | MIT | Free |
| UI | Dear ImGui | MIT | Free |
| Noise | FastNoise2 | MIT | Free |
| Physics | Jolt (optional) | MIT | Free |
| Tests | Catch2 / GoogleTest | BSL / BSD | Free |
| Serialization | Custom binary + nlohmann/json for profiles | MIT | Free |
| Hash | BLAKE3 (reference impl) | CC0 / Apache | Free |
| CI | GitHub Actions free tier (public repo) | — | Free |
| Assets | Procedural + CC0 (Kenney, Poly Haven) | CC0 | Free |
| Data | UN WPP, World Bank, OWID | Open | Free |
| IPUMS | Registration required | Free for research | Free |

**Dependency policy:** vendored via `FetchContent` or `third_party/` with pinned commits. No package manager required for a clean clone.

### 12.2 Hardware budget (NEW — target machine)

| Resource | Spec | Budget |
|---|---|---|
| CPU | Ryzen 7 (8c/16t) | 50 ms tick, 8 cores usable |
| RAM | 32 GB | Simulation ≤ 12 GB, world data ≤ 8 GB, headroom ≥ 12 GB |
| GPU | RTX 5050 | 16.6 ms frame |
| VRAM | 8 GB | Terrain ≤ 4 GB, instances ≤ 1 GB, UI ≤ 256 MB, headroom ≥ 2.7 GB |

**Scale targets:**

| Population | Tick budget | Frame budget | Status |
|---|---|---|---|
| 10 | 50 ms | 16.6 ms | Gate G9 |
| 100 | 50 ms | 16.6 ms | Gate G14 step 1 |
| 1,000 | 50 ms | 16.6 ms | Gate G14 step 2 |
| 10,000 | 50 ms | 16.6 ms | Gate G14 step 3 |
| 30,000 | 50 ms | 16.6 ms | Stretch, Phase B |

---

## 13. Testing Strategy

| Gate | System | User-visible test | Automated acceptance |
|---|---|---|---|
| G0 | Build baseline | Launch engine, capture baseline | Build succeeds; executable launches |
| G1 | Coordinate contract | Display world axes, center, corners | World↔chunk↔render round-trip within tolerance |
| G2 | Deterministic generation | Seed A twice, identical | Hashes match, 1 worker vs N workers |
| G3 | Chunk seams | Pan across boundaries zoomed in | Adjacent border samples agree within tolerance |
| G4 | Camera | Center, pan, zoom, zoom-at-cursor | Known transforms produce expected results |
| G5 | LOD | Zoom through levels | Transitions without pops beyond tolerance |
| G6 | Hydrology | Follow river source to outlet | Flow graph valid, reproducible, seams agree |
| G7 | Biomes / ecology | Inspect transitions | Classification matches input fields |
| G8 | Resources | Harvest one resource | Extraction changes only intended stock |
| G9 | 10-person population | Watch every person live | Population invariants hold each tick |
| G10 | Relationships / memory | Inspect a person's changes | Every major change has event provenance |
| G11 | Household / economy | Watch household trade | Accounting balances; transactions replay |
| G12 | Settlement | Watch residence cluster | Formation deterministic, spatially consistent |
| G13 | Replay | Rewind and compare a life path | State hash and event sequence match |
| G14 | Scale | 10 → 100 → 1,000 → 10,000 | Budgets measured; no architectural rewrite |

---

## 14. User Testing Protocol

1. Start from last known-good build. Save baseline screenshot and state hash.
2. Change exactly one subsystem.
3. Run the subsystem's diagnostic scene, not the full civilization scene.
4. Perform the fixed interaction script: center, edge, corner, pan, zoom, zoom-at-cursor, save/load, reset.
5. Run automated invariants and replay checks.
6. Freeze only after both automated and manual tests pass.
7. Then move to the next subsystem. If a later system exposes an earlier problem, return to the earlier gate.

---

## 15. Required Diagnostic Views

- Elevation / slope / normal
- Land / ocean / shoreline
- Temperature / rainfall / season
- Watersheds / flow direction / river order / lakes
- Biome suitability and transition
- Resource fields
- Chunk boundaries and IDs
- LOD level and load state
- Population density / settlement suitability
- Movement paths and current tasks
- Relationship graph around selected person
- Cognitive graph and memory provenance
- Event timeline and causal chain
- Simulation time, tick rate, queue lengths, active entities, performance counters

---

## 16. Implementation Order — Phase A and Phase B

### Phase A — Engineering (known solutions, finishable)

**Steps 1–11.** World generation, coordinates, chunks, hydrology, LOD, resources, 10-person population, needs/movement loop.

| Step | Milestone | Gate |
|---|---|---|
| 1 | Architecture freeze (this document + split docs) | — |
| 2 | Coordinate + camera contract | G1, G4 |
| 3 | Deterministic chunk generator | G2, G3 |
| 4 | Macro world import (8192×5120 as L1 field) | G2 |
| 5 | Regional procedural detail | G5 |
| 6 | Hydrology graph | G6 |
| 7 | LOD + streaming | G5 |
| 8 | Materials / ecology / resources | G7, G8 |
| 9 | Settlement suitability | G7 |
| 10 | Synthetic 10-person population | G9 |
| 11 | Needs / movement / action loop | G9 |

**Phase A is the product. Finish this first. It is shippable on its own.**

### Phase B — Research (unknown endpoints, may need revision)

**Steps 12–17.** Cognition, relationships, economy, settlement emergence, institutions, replay, scale.

| Step | Milestone | Gate | Risk |
|---|---|---|---|
| 12 | Relationships / memory / cognition | G10 | Research-adjacent |
| 13 | Households / production / exchange | G11 | Medium |
| 14 | Settlement emergence | G12 | Research |
| 15 | Institutions / culture / technology | — | Research, open-ended |
| 16 | Replay / causality at scale | G13 | Data engineering |
| 17 | Scale to 10k | G14 | Medium |

**Phase B is the experiment. Do not promise yourself Phase B. Promise Phase A, and treat Phase B as the test of the cognitive model.**

---

## 17. What Happens to the Current Vulkan Work

- The current Vulkan path becomes the renderer implementation target inside this architecture.
- The current world GPU binary becomes a transport format for macro fields, not the final terrain.
- Freeze the current working build as a baseline snapshot.
- Do not modify `terrain.frag` to compensate for camera or data-resolution problems.
- Keep `main.cpp` stable while lower-level contracts are established.
- Introduce diagnostic render modes before visual polish.
- Build chunk and coordinate contracts before richer materials.
- Only then add high-resolution terrain, hydrology visuals, vegetation, and civilization entities.

---

## 18. Anti-Patterns We Explicitly Reject

| Anti-pattern | Why it fails |
|---|---|
| One giant noise shader as the world model | Produces a picture, not a queryable world |
| Render resolution as simulation resolution | Close zoom exposes cells; forces expensive regeneration |
| Drawing rivers after terrain | Rivers become cosmetic, not infrastructure |
| Hard-coding every person | Cannot scale; prevents calibration |
| LLM per person from day one | Expensive, hard to debug, hides causal rules |
| Renderer owns game truth | Visual bugs silently become simulation bugs |
| Changing multiple systems simultaneously | Failure cannot be attributed |
| No state hashes / replay | Emergent behavior cannot be debugged |
| No versioned generation | Changing a noise parameter mutates existing civilizations |
| Untracked dependencies | A clean clone must build identically |
| Silent coordinate reinterpretation | Breaks determinism and seams |
| Non-deterministic RNG shared across systems | Breaks replay and parallel determinism |

---

## 19. Research Notes and Sources

1. The Word of Notch, 9 March 2011 — on-the-fly generation, chunks, determinism, layered noise, local coordinates. https://blog.omnichive.uk/post/3746989361/terrain-generation-part-1/
2. Joshua Tippetts — *Creator of worlds*. Game Developer, April 2011. Chunk-based procedural worlds, base + modifications. https://media.gdcvault.com/GD_Mag_Archives/GDM_April_2011.pdf
3. Genevaux et al. — *Terrain generation using procedural models based on hydrology*. ACM TOG, 2013. https://doi.org/10.1145/2461912.2461996
4. Peytavie et al. — *Procedural Riverscapes*. CGF, 2019. https://onlinelibrary.wiley.com/doi/10.1111/cgf.13814
5. Grenier et al. — *Real-time Terrain Enhancement with Controlled Procedural Patterns*. CGF, 2024. https://onlinelibrary.wiley.com/doi/10.1111/cgf.14992
6. UN World Population Prospects 2024. https://population.un.org/wpp/downloads/
7. IPUMS International 7.6. https://www.ipums.org/projects/ipums-international/d020.7.6
8. Our World in Data — Population sources. https://ourworldindata.org/population-sources
9. Thalren Vale — minimalist agent-based civilizational emergence on consumer hardware.
10. Orbis — solo C++20 / Vulkan civilization simulation.
11. Project Sid — LLM-powered multi-agent Minecraft civilization (contrast case).
12. Dwarf Fortress — needs, memory, relationships, emergent narrative at scale.

---

## 20. Definition of Done

### Phase A (engineering)

- Coordinate spaces explicit and independently tested.
- Seed + generation profile produces reproducible world data.
- World generation hierarchical and chunk-addressable.
- Hydrology authoritative and seam-consistent.
- Rendering LODs independent; no raw macro cells exposed at arbitrary zoom.
- Simulation owns truth, state, events, persistence.
- 10-person synthetic population from statistical distributions.
- Each subsystem has a diagnostic view and deterministic acceptance test.
- All Phase A gates pass.

### Phase B (research)

- Relationships, memory, cognition inspectable with provenance.
- Households and economy balance and replay.
- Settlement formation deterministic and spatially consistent.
- Replay reproduces selected histories bit-exactly.
- Scale to 10,000 agents within tick and frame budgets.
- All Phase B gates that are met pass; unmet gates documented as open research questions.

Only after Phase A gates pass do we proceed to richer assets, civilization content, and scale optimization.

---

## 21. Decision Log Requirements (NEW)

Every non-trivial decision must be recorded in `docs/DECISIONS.md` with:

- Date
- Decision
- Alternatives considered
- Reason
- Reversibility
- Related gate or spec

This is how the project remains coherent across months of work.

---

## 22. Documentation Layout (NEW)

```
docs/
  ARCHITECTURE.md          // this document
  COORDINATES.md           // §4 expanded
  WORLDGEN.md              // §5–6 expanded
  HYDROLOGY.md             // §6.1 expanded
  CHUNKS.md                // §6.2 expanded
  SAVE_FORMAT.md           // §6.3 expanded
  COGNITION.md             // §8.2 expanded — its own doc
  ECONOMY.md               // §8.4 expanded
  EMERGENCE.md             // §8.5 expanded, research notes
  DETERMINISM.md           // §11.2 expanded
  EVENTS_REPLAY.md         // §11.1 expanded
  BUILD.md                 // §12.1 expanded
  HARDWARE_BUDGET.md       // §12.2 expanded
  TESTING.md               // §13–14 expanded
  DIAGNOSTICS.md           // §15 expanded
  ROADMAP.md               // §16 expanded
  PERFORMANCE.md           // §15 + scale targets
  DECISIONS.md             // §21
  STYLE.md                 // C++ style, naming, includes
  LAYOUT.md                // directory tree, module boundaries
```

No gate is defined until its inputs, outputs, data model, and hashable state are specified in the corresponding doc.

---

**Baseline status:** Architecture planning complete for Phase A. Phase B is specified at the data-model level and marked as research. Implementation changes remain paused until this revision is accepted and `docs/` is populated.

---