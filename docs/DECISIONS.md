# Axiom — Architectural Decisions

This document records decisions that are expensive to change later.

---

## D001 — Simulation Tick Rate
- 10 Hz fixed tick

## D002 — Spatial Model
- 2D grid, side-view, gravity acts in -Y

## D003 — Cell Size
- 1 m³ per cell

## D004 — Determinism Policy
- Determinism first, replayable

## D005 — Fixed-Point Policy
- Global numeric policy
- Must be resolved before real physics systems

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
