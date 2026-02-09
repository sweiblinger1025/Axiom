# ARCHITECTURE.md — Axiom Core + App Shells

**Version:** 0.4  
**Status:** LOCKED  
**Last Updated:** 2026-02-08  
**Depends On:** VISION.md v0.3 (LOCKED), DECISIONS.md (ACTIVE)

---

## Purpose

Define the **high-level architecture** for the Axiom 3D restart:
- what lives in **Axiom Core** vs **app shells**
- how input flows in (Actions/Intents) and how state flows out (Snapshots)
- how we preserve **logic determinism** under a fixed-tick simulation loop
- how we structure modules so Milestones A1/A2/B can be implemented without rewriting the foundation

This document does **not** specify gameplay content, quest systems, or tools beyond what is required to wire the core loop.

---

## Scope

In scope for v1 architecture:
- Core library boundaries and ownership rules
- Fixed-tick simulation loop and determinism constraints
- Physics integration strategy (queries + character controller truth)
- Content records (simple format) and stable ID strategy (shape, not final schema)
- Save/load plumbing and versioning strategy (shape, not final binary layout)
- Snapshot model for apps (render/debug/headless)

---

## Non-Goals

- Choosing the final physics backend (library vs custom) — that’s an Open Question
- Designing a full “Creation Kit” editor
- Defining full rendering pipeline, animation system, or asset pipeline
- Implementing complex AI, quest logic, or dialogue systems (beyond milestone needs)
- Solving spatial determinism across machines

---

## Definitions

- **Core / Axiom Core:** the C++ library that owns authoritative gameplay state and rules.
- **App Shell:** an executable that hosts Core, provides rendering/UI/input, and calls into Core through the C ABI.
- **Headless Shell:** an app shell with **no renderer** used for determinism tests, replay validation, and CI.
- **Action/Intent:** typed, versioned input messages from the app to Core (movement, look, fire, reload, use).
- **Snapshot:** read-only state exported from Core for rendering, debug UI, tests, and replay.
- **Tick:** one fixed simulation step (rate TBD; see D106).

(See `GLOSSARY.md` once drafted; this doc assumes the same meanings.)

---

## Architecture Overview

### Ownership boundary (hard rule)

- Core owns **truth**: world state, character state, rules, events, persistence.
- Apps own **presentation**: rendering, animation, audio, UI, camera behavior, and input sampling.
- Physics backend is **subordinate**: Core decides movement and resolution; physics provides collision queries.

This is the practical interpretation of D103 and D107.

### Physics integration point (concrete)

- The physics backend is integrated into **Core**, not the app shell.
- Core owns the physics context needed for collision queries and calls it during tick processing.
- Apps never link against or call the physics backend directly; apps only see snapshots and events.

(Which backend to use is still open, but *where it lives* is fixed by this architecture.)

### Data flow (one-way each direction)

1) App samples input → emits **Actions/Intents** with a tick stamp (or “apply next tick”).
2) Core consumes actions → advances one **tick** → mutates truth.
3) Core emits **Snapshot** + event notifications for UI/logging.

Core never calls into the app. App never mutates truth directly.

---

## Actions (minimum shape)

Actions are:
- **typed** (tagged union / discriminated struct)
- **versioned** (so the ABI can evolve)
- submitted in **batches** through the C ABI

Pattern-wise, this matches the 2D prototype’s “command model” style, but the payloads are 3D-centric.

For A1/A2, the minimum action set includes:
- `MoveIntent` (2D input vector)
- `LookIntent` (yaw/pitch deltas or absolute)
- `FirePressed` / `FireReleased` (or `FireOnce` for v1)
- `Reload`
- (optional) `SprintHeld`, `CrouchToggle`

Exact struct layout lives in `WORLD_INTERFACE.md`.

Action validation follows the 2D prototype pattern: **structural validation at submission**, and **state-dependent validation at tick execution** (logic-deterministic).

---

## Snapshot Model (minimum content)

Snapshots must be sufficient for A1/A2/B without requiring an editor or direct world mutation.

For A1 (shooting range), snapshots must expose at minimum:
- entity transform: position + rotation (player + targets)
- entity HP (targets) and player HP (if used)
- player weapon state: ammo count + reload state
- a tick counter / sim time
- emitted events (damage dealt, reload, target destroyed)

Snapshot *packaging* (single blob vs channels vs views) is still an open design choice, but the **content** above is non-negotiable for milestone implementation.

---

## Core Library Structure (conceptual modules)

These are conceptual modules; actual filenames/folders can differ.

### 1) `ax_core` — runtime + lifecycle
Responsibilities:
- engine instance creation/destruction
- tick stepping (`step_n_ticks`)
- action queue ingestion
- snapshot production
- version stamping and diagnostics (build hash, protocol versions)

### 2) `ax_world` — authoritative state
Responsibilities:
- authoritative world state storage required by current milestones
- stable IDs for entities and placed references
- “spaces” concept (structure only)

Entity representation is an open design choice. v1 needs at minimum:
- a player entity
- static target entities with HP (A1)
- one AI entity (A2)
- stable IDs sufficient for save/load and event attribution

The simplest representation that supports A1 wins (do not “ECS-ify” prematurely).

**Space model:** A1/A2 run in a single active space. Multi-space support (hub/interior/route) is added for Milestone B.

### 3) `ax_sim` — rules + systems
Responsibilities:
- rule evaluation per tick (movement, combat resolution, events)
- deterministic ordering rules
- RNG policy hooks (seeded streams; see RNG note below)

### 4) `ax_physics_iface` — collision queries + character controller support
Responsibilities:
- raycast / shape cast / overlap query API
- broadphase filters / layers
- character controller query support (sweeps, grounding, step/slope checks)
- optional debug data export (data only)

Notes:
- `ax_physics_iface` defines the query API Core calls. The backend implementation behind it is swappable.
- v1 truth is a **character controller owned by Core**.
- Physics backend must not become the owner of authoritative position/velocity.

### 5) `ax_content` — records and lookups
Responsibilities:
- parse/load a **simple record format** (v1)
- stable IDs for records used by milestones (weapon defs, target archetypes, AI archetypes)
- lookups for sim systems (combat, spawning, interactions)

Notes:
- Override layering is explicitly v2 (VISION pillar). v1 is “one content set.”
- Records must be sufficient to define all A1/A2/B content without an editor.

### 6) `ax_save` — serialization + migrations
Responsibilities:
- save header with version + build info
- serialize/deserialize world truth
- migration pipeline between save versions (explicit, testable)

**Dependency rule:** content is loaded before save state. Saves reference content by stable ID.
Missing content references are handled by a defined policy (TBD in `SAVE_FORMAT.md`).

### 7) `ax_events` — event dispatch
Responsibilities:
- structured events emitted by systems (damage dealt, reload, target destroyed, contract chosen, heat changed)
- event queue exported to apps/tests

Notes:
- Events are gameplay-facing hooks. Apps may display them, log them, or ignore them.

### 8) `ax_debug` — diagnostics + inspection (small but deliberate)
Responsibilities:
- diagnostics export (build hash, protocol versions)
- debug inspection helpers (entity dumps, invariant checks, save integrity checks)
- test helpers used by headless shells (not a UI module)

This can start as part of `ax_core` but is called out so it doesn’t get lost.

---

## RNG Policy (logic determinism support)

RNG must not accidentally couple outcomes to app timing or input batching.

- v1 policy: RNG is owned by Core and advanced only during ticks.
- Detailed policy (global stream vs per-system streams) is deferred until the first feature that needs it (A2 AI). When chosen, it must be deterministic under identical tick-stamped actions.

---

## C ABI Boundary (D101)

Core exposes a stable C ABI so app shells do not depend on C++ ABI details.

### Minimum required calls (v1)

- `ax_create(...)` / `ax_destroy(...)`
- `ax_load_content(...)` / `ax_unload_content(...)`
- `ax_submit_actions(...)`
- `ax_step_ticks(n)`
- `ax_get_snapshot(...)` (copy or view semantics, but deterministic)
- `ax_save(...)` / `ax_load_save(...)`
- `ax_get_diagnostics(...)` (versions, build hash, feature flags)

Exact signatures are defined in `WORLD_INTERFACE.md` and `SAVE_FORMAT.md`.

---

## Simulation Model

### Fixed tick (D106)

- Core advances only by ticks; apps may render at any framerate.
- Actions are applied at deterministic boundaries (tick N), not “mid-frame.”
- Tick rate is **TBD**. Do not assume 10 Hz from the 2D prototype.

### Determinism (D102)

- **Logic determinism is guaranteed**: given identical initial state + identical tick-stamped actions, truth transitions are identical.
- **Spatial determinism** is not guaranteed; validation must focus on logic outcomes.
- Replay tests can optionally snapshot spatial state at defined decision points to stabilize comparisons.

---

## Milestone Mapping (what this architecture must support)

### A1 — Shooting Range
- one player controller, aim + hitscan
- static targets with HP
- ammo + reload logic
- save/load + replay of logical outcomes

### A2 — Combat Arena
- one AI opponent (perception + engage + simple cover selection)
- same replay/save guarantees as A1

### B — RPG Slice
- multi-space model (hub + interior + route)
- minimal branching interaction (2–3 choices)
- persistence of changes

---

## Acceptance Criteria

This doc is “good enough to proceed” when:

- A developer can implement the **Core ↔ App** boundary without guessing ownership rules.
- A developer can build A1 (shooting range) without inventing new architecture.
- Save/load and replay fit cleanly into the core loop without special-casing “tests.”
- The physics backend is clearly defined as a **query provider**, integrated into Core, not the owner of truth.
- Headless shells are explicitly supported as first-class app shells for tests and CI.

---

## Open Questions

- What physics backend will v1 use (custom queries vs library)?  
- What are the coordinate and unit conventions (meters vs centimeters, handedness, gravity constant)?  
- What is the persistence granularity (every entity vs flagged-only persistence)?  
- What is the initial content record format (JSON, custom binary, or other)?  
- What is the v1 entity representation (minimal ECS vs bespoke structs), and how does it stay debuggable and save-friendly?  
- What are the snapshot semantics (copy vs shared immutable buffer) and lifetime rules across the C ABI?  
- How does the app shell handle render-frame interpolation between sim ticks (dual snapshots, velocity extrapolation, or other)?  
- What is the v1 save container strategy (single blob vs chunked sections), and how are migrations tested?  
- What threading model do we allow in v1 (recommended: single-threaded sim; any parallelism must not break determinism)?  
- What is the missing-content policy when loading a save that references absent content records?  

---

## ChangeLog

### v0.1
- Initial draft: defines Core/app boundary, module responsibilities, C ABI surface, fixed-tick loop, and milestone mapping.

### v0.2
- Made headless app shells explicit as first-class consumers of the C ABI for tests/CI.
- Clarified physics backend location: integrated into Core; apps never call physics directly.
- Added minimum “shape” for Actions and minimum required Snapshot content for A1/A2/B.
- Tightened `ax_world` description to acknowledge entity model is open and to prefer the simplest model that supports A1.
- Clarified space model staging: A1/A2 single space; multi-space support added for Milestone B.
- Added explicit `ax_content` ↔ `ax_save` dependency rule (content loads before saves; saves reference content by ID).
- Added a small `ax_debug` module responsibility callout for diagnostics/inspection.
- Added an RNG policy note framing determinism constraints without over-specifying the implementation.

### v0.3
- Added app-shell render interpolation as an explicit Open Question.
- Clarified `ax_physics_iface` as an abstract query API with swappable backend implementations.
- Stated that action validation uses the two-phase pattern: structural validation at submission, state-dependent validation at tick.

### v0.4
- Added persistence granularity and content record format to Open Questions (carried from VISION).
- Merged `ax_physics_iface` Note/Notes into a single Notes block.
