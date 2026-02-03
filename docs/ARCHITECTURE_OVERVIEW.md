# Axiom — Architecture Overview

This document defines the high-level architecture, invariants, and layering of the Axiom engine.

Axiom is a deterministic, simulation-first engine designed for an ONI-style 2D side-view colony game.
It separates **truth** (C++) from **presentation** (C#) via a stable C ABI boundary.

If any implementation choice conflicts with this document, **this document wins**.

---

## Core Goals

- Deterministic simulation (replayable, debuggable, multiplayer-ready)
- Clear separation of truth vs presentation
- Data-driven variation via world profiles
- Long-term maintainability for a hobby/learning project

---

## Architectural Layers

- **C++ Core (Truth)**
  - Simulation state, rules, determinism
- **C ABI Boundary**
  - Stable binary interface (`ax_*`)
- **C# Viewer**
  - Rendering, UI, unit conversion, tooling
- **Data**
  - Profiles, materials, localization

Lower layers never depend on higher layers.

---

## Core Invariants

1. C++ owns truth
2. All truth changes occur via commands
3. Fixed-tick simulation (10 Hz)
4. Canonical SI units internally
5. Determinism is mandatory
6. Physics is constant; variation is parameterized

---

## Global Conservation Rules

All systems must conserve mass and energy.
Any creation or destruction must be explicit and auditable.
Implicit drift is a bug.

---

## Repo Layout (Authoritative)

```
engine/
  core/
  bindings/
  include/axiom/
  src/
  shared/

apps/
  headless/
  viewer/

docs/
```

---

## Summary

Axiom’s architecture exists to protect correctness and clarity over time.
