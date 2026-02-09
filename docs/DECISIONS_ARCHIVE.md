# DECISIONS_ARCHIVE.md — Prior Architecture (2D Prototype)

**Status:** ARCHIVE (Historical Reference Only)  
**Last Updated:** 2026-02-08  

This file preserves decisions made during the prior **2D ONI-style prototype** era (e.g., D001–D013).
They are retained for historical context and rationale, but are **not active** for the 3D restart.

**Rule:** Treat everything in this file as **superseded** unless a new active decision explicitly re-adopts it
in `DECISIONS.md`.

---

## Archived Decisions (2D Prototype Era)

## D001 — Simulation Tick Rate
- 10 Hz fixed tick

## D002 — Spatial Model
- 2D grid, side-view, gravity acts in -Y

## D003 — Cell Size
- 1 m³ per cell

## D004 — Determinism Policy
- Determinism first, replayable

## D005 — Canonical Fixed-Point Type
**Decision:** Domain-specific scaled integers.

**Locked Types (v0):**
- Temperature: `ax_temp_mK` (int32, milliKelvin)
- Mass: `ax_mass_mg` (int64, milligram)
- Energy: `ax_energy_mJ` (int64, milliJoule)

**Overflow Policy:**
- Debug: assert/trap
- Release: saturate to type bounds

**Division:** Truncate toward zero by default; explicit floor/round variants available.

**Floating-Point:** Forbidden in math utilities.

**Rationale:**
Domain-specific types are self-documenting and force explicit dimensional handling.
Scaled integers (not universal fixed-point) allow optimal range/precision per quantity.
Integer-only arithmetic guarantees cross-platform determinism.

**Locked by:** TASK-004

## D006 — Memory Layout
- SoA for per-cell fields (default)

## D007 — Chunking
- Fixed-size square chunks

## D008 — Snapshot Strategy
- Channel-based, full + delta

## D009 — Event Semantics
- Deterministic ordering, no same-tick cascading

## D010 — Hobby Framing
- Habitat is testbed; engine work allowed for learning

## D011 — Viewer Tech
- Visual Studio 2022 (.NET 8)

## D012 — Threading
- Single-threaded v0, parallel-ready model

## D013 — Serialization Versioning
- All serialized formats versioned

### Notes on Supersession
- D005 (fixed-point math types) is not re-adopted for the 3D restart; numeric policy will be revisited as needed in future architecture docs.
- If an archived decision conflicts with an active decision (D100+), the active decision wins.
- Examples:
  - Any fixed tick rate (e.g., 10 Hz) from the 2D era is superseded by **D106** (fixed tick, rate TBD).
  - Any 2D grid world assumptions are superseded by **D100** (3D RPG primary target).
