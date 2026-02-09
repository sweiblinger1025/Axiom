# DECISIONS.md — Active (3D Restart)

**Status:** ACTIVE  
**Last Updated:** 2026-02-08  

This file is the **active decision log** for the Axiom **3D restart** (Fallout-style RPG primary target).
Older decisions from the prior 2D prototype (D001–D013) are preserved for history in `DECISIONS_ARCHIVE.md`
and should be treated as **superseded** unless explicitly re-adopted.

---

## D100 — Primary Target: 3D Fallout-Style RPG
**Decision:** Axiom’s primary v1 target experience is a 3D first/third-person RPG (Fallout-like), not the prior 2D colony sim.  
**Rationale:** This pivot drives every expensive-to-reverse architecture choice (spatial model, movement, combat kernel, content layout).  
**Locked by:** VISION v0.3  

## D101 — Core Library + Separate Apps (C ABI Boundary)
**Decision:** Axiom is built as a core library with separate application shells, and the core exposes a stable C ABI boundary for apps/tools integration.  
**Rationale:** Keeps “truth” isolated, enables multiple proof apps (including headless), and reduces coupling/ABI churn during iteration.  
**Locked by:** VISION v0.3  

## D102 — Determinism Tier Policy
**Decision:** Logic determinism is guaranteed in v1; spatial determinism is a stretch goal and not guaranteed.  
**Rationale:** Makes determinism testable and honest for 3D gameplay without requiring a fully deterministic physics stack immediately.  
**Locked by:** VISION v0.3  

## D103 — Truth/Presentation Split Applies to 3D
**Decision:** Axiom owns authoritative gameplay state (including character state such as position/velocity/intents); apps own rendering and animation and only send actions/intents.  
**Rationale:** Prevents engine bloat and keeps simulation stable, testable, and independent of renderer/animation choices.  
**Locked by:** VISION v0.3  

## D104 — Save Versioning + Migrations From Day One
**Decision:** All saves are versioned and migrations are explicit and testable from the first implementation.  
**Rationale:** Prevents “save rot” and supports long-term evolution without bricking.  
**Locked by:** VISION v0.3  

## D105 — Windows-First (No Portability Work in v1)
**Decision:** Windows is the only supported platform target for v1, and no time is spent on Linux/macOS portability work.  
**Rationale:** Scope discipline for a solo hobby/learning project; portability is deferred until the architecture stabilizes.  
**Locked by:** VISION v0.3  

## D106 — Tick-Based Simulation Core
**Decision:** Axiom’s simulation advances on a fixed tick (not frame-driven), and gameplay logic is evaluated against ticks.  
**Rationale:** Preserves logic determinism goals and keeps simulation viable independent of render framerate. **Tick rate is intentionally TBD; 10 Hz from the 2D prototype is not assumed to carry forward.**  
**Locked by:** VISION v0.3  

## D107 — No Engine-Owned Animation
**Decision:** The engine does not own an animation system in v1; apps handle animation while Axiom tracks gameplay state only.  
**Rationale:** Avoids major scope creep and keeps the core focused on authoritative state and rules.  
**Locked by:** VISION v0.3  

## D108 — ABI Versioning Uses MAJOR/MINOR
**Decision:** The Axiom Core C ABI uses an explicit MAJOR/MINOR version split (not a single integer) for compatibility control.
**Rationale:** MAJOR/MINOR enables additive evolution without breaking shells while still making breaking changes explicit; changing ABI versioning strategy later is expensive.
**Locked by:** WORLD_INTERFACE v0.2
**Shape:** `ax_get_abi_version()` returns `{major, minor}` as two `uint16_t` values.

## D109 — Snapshots Use Copy-Out in v1
**Decision:** v1 snapshot access is copy-out only (no borrowed immutable view).
**Rationale:** Simplifies lifetime/ownership rules and eliminates view invalidation hazards; sufficient for A1/A2/B.
**Locked by:** COMBAT_A1 v0.2

## D110 — A1 Hitscan Uses Truth Look Direction
**Decision:** In A1, the Fire action carries no aim vector; Core computes hitscan ray using the player entity’s truth orientation at tick execution.
**Rationale:** Keeps actions minimal, keeps truth authoritative, and avoids app-camera divergence becoming a logic input.
**Locked by:** COMBAT_A1 v0.2

## D111 — Reload Durations Are Authored in Ticks
**Decision:** Weapon reload durations are authored and simulated as integer tick counts in Core.
**Rationale:** Tick rate is TBD (D106); integer ticks avoid float-time determinism concerns and keep Core logic simple.
**Locked by:** COMBAT_A1 v0.2

## D112 — A1 Uses Next-Tick Action Scheduling
**Decision:** For Milestone A1, shells submit actions for `tick = current_tick + 1` only.
**Rationale:** Simplifies the first implementation path while preserving absolute tick stamping in the ABI for later needs.
**Locked by:** COMBAT_A1 v0.2

