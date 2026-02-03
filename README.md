# Axiom

**Axiom** is a deterministic, simulation-first engine for an ONI-style 2D side-view colony game.

The project emphasizes:
- correctness over convenience
- clear separation of simulation truth and presentation
- replayable, debuggable systems
- long-term maintainability for a hobby / learning project

Axiom is built as a hybrid:
- **C++** for simulation truth
- **C ABI** as a stable boundary
- **C# (.NET 8)** for rendering, UI, and tooling

This repository contains both the engine and a reference game project (*Habitat*) used as a testbed.

---

## Project Status

**Early development / Milestone 0**

At this stage the focus is on:
- validating the toolchain and ABI boundary
- locking architectural contracts
- establishing a clean foundation before gameplay systems are implemented

No gameplay systems are complete yet.

---

## Repository Structure

```
engine/
  core/                 # Simulation truth (C++)
  bindings/
    c/                  # C ABI boundary (ax_*)
  include/axiom/        # Public C++ headers
  src/                  # Internal engine implementation
  shared/               # Shared utilities

apps/
  headless/             # C++ headless host (tests, benchmarks)
  viewer/               # C# viewer / UI / tools (.NET 8)

data/
  profiles/             # World/environment profiles
  content/              # Materials, devices, structures
  localization/         # Tone and string variants

games/
  habitat/              # Primary game project using Axiom

tests/
  core_tests/           # Deterministic C++ tests
  integration/          # End-to-end tests

docs/                   # Architecture and design documents
```

---

## Architecture Overview

Axiom is built around a few core ideas:

- **Truth vs Presentation** — C++ owns simulation truth. C# owns rendering, UI, unit conversion, and tooling. All interaction crosses a stable C ABI boundary.
- **Command-Driven Simulation** — All changes to truth occur via validated commands, applied deterministically at fixed tick boundaries.
- **Determinism First** — Fixed 10 Hz tick rate, canonical SI units internally, replayable command streams.
- **Data-Driven Variation** — World behavior varies via profiles. Physics equations remain constant.

For full details, see the documents in `docs/`.

---

## Documentation

The `docs/` directory contains the authoritative architecture and design contracts:

| Document | Purpose |
|---|---|
| `ARCHITECTURE_OVERVIEW.md` | High-level structure and invariants |
| `DECISIONS.md` | Locked decisions with rationale |
| `TOOLCHAIN.md` | Build system and Milestone 0 |
| `TRUTH_VS_PRESENTATION.md` | C++ / C# boundary rules |
| `SPATIAL_MODEL.md` | Grid, indexing, chunking |
| `COMMAND_MODEL.md` | Command lifecycle and determinism |
| `UNIT_SYSTEM.md` | Canonical units and numeric policy |
| `SNAPSHOT_EVENT_FORMATS.md` | Snapshot and event contracts |
| `WORLD_PROFILES.md` | World parameterization |

If documentation and code disagree, **the documentation wins**.

---

## Tooling

| Layer | Tool |
|---|---|
| C++ engine | CLion (CMake) |
| C# viewer | Visual Studio 2022 (.NET 8) |
| Documentation | VS Code |

This separation mirrors the engine's architectural boundaries.

---

## Build (Milestone 0)

> Full instructions will be in `docs/TOOLCHAIN.md` once Milestone 0 is complete.

Milestone 0 exists solely to validate the ABI boundary and toolchain, not to implement gameplay.

### C++ (repo-root CMake)

```bash
cmake -S . -B build
cmake --build build
```

This produces:
- `AxiomCore` — shared library
- `ax_headless` — C++ headless host

### C# viewer

```bash
cd apps/viewer
dotnet build
```

### Requirements

- CMake >= 3.24
- C++20 compatible compiler (MSVC recommended on Windows)
- .NET 8 SDK

---

## Goals (Longer Term)

- Implement deterministic simulation systems (thermal, gases, liquids)
- Develop a playable ONI-style colony prototype
- Explore co-op and replay features
- Keep the engine clean, understandable, and hackable

This is a learning-driven project — correctness and clarity matter more than speed.

---

## Non-Goals (for Now)

- No gameplay balance or content focus yet
- No modding API
- No scripting layer
- No networking until single-player determinism is proven

---

## License

TBD

---

## Contributing

This is currently a personal project. Contribution guidelines may be added later.