# VISION.md — Axiom Engine (Docs-First Restart)

**Version:** 0.3  
**Status:** LOCKED  
**Last Updated:** 2026-02-08  

---

## Purpose

Axiom is a **deterministic game kernel** implemented as a **core C++ library** with **separate application shells**. Its job is to provide a stable, testable foundation for a **Fallout-style 3D RPG** (first/third-person, spatial combat), while keeping the architecture capable of supporting **simulation-heavy modes** later (ONI-like colony systems).

Axiom exists to make "the world is a database" gameplay feasible for a hobby engine:
- **persistent objects**
- **save versioning + migrations**
- **repeatable logic outcomes** under identical inputs
- **spatial gameplay** (3D movement, shooting, interiors)

---

## Context: Engine Restart

Axiom previously existed as a **2D ONI-style colony sim prototype**. That work covered early tasking (TASK-000 through TASK-004) and established useful foundations: a determinism mindset, a truth/presentation split, and a fixed-point math policy.

This document marks a **fresh architectural start** pivoting to a **3D Fallout-style RPG** as the primary target. Most prior implementations are being replaced:
- the **2D grid spatial model**
- the colony-sim world layout assumptions
- any "cell physics" ideas tied to grid simulation
- thermal primitives as a near-term driver (they remain valuable, but not the v1 engine focus)

What carries forward are principles, not code:
- **truth vs presentation separation**
- **deterministic logic as a design constraint**
- **data-driven content**
- **save versioning with migrations**

---

## Vision Statement

**Axiom is the truth.**  
Apps render the world, run animation, and collect input. Axiom simulates the world, resolves actions, and produces snapshots.

Axiom prioritizes:
- correctness and maintainability
- deterministic **logic** outcomes
- stable identifiers and persistence
- saves that survive engine evolution

Axiom does not optimize first for:
- photoreal rendering
- a full editor toolchain
- high-end physics (ragdolls/cloth/destruction)
- shipping a commercial product

---

## Primary Target Experiences

### 1) Corridor RPG (Fallout 4–inspired feel)
A first/third-person 3D RPG in a constrained region with:
- spatial exploration (exteriors + interiors)
- real combat encounters (not abstract cards)
- factions, reputation, and "heat"
- quests/contracts and POIs
- persistence (world state accumulates)

The tone is grounded, faction-driven, systemic—not purely scripted.

### 2) Colony / Simulation Mode (ONI-like direction)
A simulation-forward mode emphasizing:
- tick-based systemic simulation
- cell-based scalar fields (reimplemented as needed, not carried from the 2D prototype)
- long-running progression and reproducible outcomes

This mode is not required for v1 milestones. The architecture must not block it.

---

## Non-Goals (v1 restart scope cuts)

These are hard "no" items for the restart:

- No production-quality renderer or lighting pipeline
- No full "Creation Kit" editor in v1
- No authoritative rigidbody physics (ragdolls/cloth/stacking/destruction)
- No procedural world generation (world is hand-authored)
- No dialogue system in v1 (beyond minimal branching interactions)
- No inventory/crafting system beyond what combat requires (ammo + a few counters)
- No engine-owned animation system (apps own animation; Axiom only tracks gameplay state)
- No multiplayer/networking
- No cross-platform shipping requirements (Windows-first is fine)

---

## Guiding Principles

1) **Truth / Presentation Separation**  
   Only Axiom mutates authoritative world state. Apps send Actions/Intents and render Snapshots.

2) **Determinism Tiers (v1 policy)**
   - **Logic determinism:** identical initial state + identical inputs ⇒ identical game-state transitions. **Guaranteed.**
   - **Spatial determinism:** bit-identical positions/contacts across runs. **Stretch goal, not guaranteed.**

   Replay and test strategy must rely on **logic determinism**, with optional spatial snapshotting at defined points to stabilize validation.

3) **Saves Must Survive**  
   Save files are versioned from day one, with explicit migrations.

4) **Data-First, Stable IDs First**  
   v1 requires stable IDs and a simple record format. Override layering (base/DLC/mod load order) is **v2**.

5) **Small, Always-Runnable Proof Apps**  
   Engine progress is validated by small apps that never rot:
   - shooting range
   - combat arena
   - RPG slice
   - (later) grid sim slice

6) **Compatibility Intent: RPG + Sim**  
   The architecture should be able to host both:
   - 3D spatial queries for RPG gameplay, and
   - cell-based scalar fields for simulation mode.

   This is a design intent, not a v1 promise. The concrete constraints live in core architecture docs.

---

## Core Pillars (v1)

Axiom v1 must provide:

1) **Stable IDs + Persistence**
   - persistent world references across saves
   - explicit policy for what is persistent vs ephemeral (see Open Questions)

2) **Content Records with Stable IDs**
   - a simple record format in v1
   - override layering is explicitly deferred to v2
   - records are sufficient to define all A1/A2/B content without an editor

3) **Save Versioning + Migrations**
   - every save has a version header
   - migrations are explicit and testable

4) **3D Spatial Movement + Combat Kernel**
   - Axiom owns authoritative character state (position/velocity/intents)
   - hitscan weapons via raycasts
   - basic damage and status

5) **Event System**
   - event-driven hooks for quests/contracts, AI triggers, and world interactions

6) **Architecture Support for Cell/Chunk Streaming**
   - the design supports streaming/cells
   - implementation is not required for the earliest combat milestones

---

## Success Criteria (Milestone Targets)

### Milestone A1: "Shooting Range" (first)
A small 3D testbed where:
- player can move, aim, shoot (hitscan), reload
- damage + ammo counters work
- save/load works
- replay test reproduces key **logical outcomes** (ammo remaining, damage totals against static targets)

### Milestone A2: "Combat Arena"
Extends A1 with:
- one AI opponent that can perceive and engage
- basic cover-seeking behavior
- save/load works in-combat
- replay test reproduces key **logical outcomes**, using recorded inputs and optional spatial snapshots at defined points

### Milestone B: "RPG Slice"
A small region slice where:
- one hub + one interior + one travel route exist
- at least one contract interaction exists with minimal branching (2–3 choices max)
- persistence is demonstrated (moved/changed objects remain changed)
- event/encounter outcomes affect world state (faction heat, patrol pressure, vendor availability)

---

## Open Questions (for follow-up docs)

**App Shell / Rendering**
- What is the v1 app shell rendering stack? Leading candidates are SDL2 + OpenGL (fastest path to testing Axiom) or Vulkan (deeper learning investment, slower path to A1). This is a hobby/learning project — learning value matters alongside velocity.
- Does the headless runner pattern from the 2D prototype carry forward for non-visual engine testing?

**3D Simulation + Physics**
- What tick rate is used for truth simulation in 3D gameplay? (10 Hz from the 2D prototype is not assumed to carry forward.)
- What physics backend is used for collision queries and raycasts (custom vs library such as Jolt, Bullet, PhysX)?
- What is the minimum collision representation for reliable raycasts (tri-mesh, convex sets, hybrid)?
- Where is the authority boundary between Axiom's gameplay state and the physics backend's spatial state?
- What are the coordinate and unit conventions (meters vs centimeters, handedness, gravity constant)?

**Persistence**
- What is the persistence granularity (everything vs flagged objects only)?
- How are unloaded cells stored (delta from authored baseline vs full snapshot per cell)?
- What is the save format strategy for 3D world state (binary vs structured, chunked vs monolithic)?

**World Structure**
- How are interiors represented (separate spaces vs portal-linked zones)?
- When does cell/chunk streaming become required (Milestone B+), and what is the minimum viable streaming model?

**Camera**
- What is the initial camera mode priority (third-person first vs first-person first)?

**Content Data**
- What is the initial record format (JSON first vs custom)?
- What is the stable ID scheme for records and placed references?

These will be resolved in core docs: `ARCHITECTURE.md`, `WORLD_INTERFACE.md`, `SAVE_FORMAT.md`, `CONTENT_DATABASE.md`, `WORLD_STREAMING.md`, and `COMBAT.md`.

---

## ChangeLog

### v0.1
- Initial draft: core library + separate apps, FO4-inspired pillars, determinism/persistence focus, milestone targets.

### v0.2
- Added explicit restart context and clarified what carries forward vs what is replaced from the prior 2D prototype.
- Replaced "deterministic enough" with a tiered determinism policy (logic guaranteed, spatial stretch).
- Split Milestone A into A1 (Shooting Range) and A2 (Combat Arena) with realistic scope.
- Tightened v1 non-goals (no procgen, no dialogue system, no full inventory/crafting, no engine-owned animation).
- Reframed streaming and content layering as architecture-supported in v1, implemented later.
- Expanded Open Questions for persistence granularity, unloaded cell storage, physics backend, and save strategy.

### v0.3
- Clarified colony "cell-based scalar fields" are reimplemented, not reused from the 2D prototype.
- Tightened the RPG+Sim compatibility statement to "intent," removing the implied constraint.
- Restored "Content Records with Stable IDs" as a v1 pillar and kept override layering deferred to v2.
- Relaxed Milestone B interaction scope to minimal branching (2–3 choices) instead of accept/decline only.
- Re-added camera mode priority to Open Questions.
- Added App Shell / Rendering open questions (rendering stack choice, headless runner pattern).
- Added physics authority boundary to Open Questions.
- Noted tick rate from 2D prototype is not assumed to carry forward.
- Locked document (set Status to LOCKED; removed v0.3-final header).
- Tightened Milestone A1 replay criteria to static targets only.
- Added coordinate/unit conventions to Open Questions.
- Clarified records are sufficient for A1/A2/B content without an editor.

