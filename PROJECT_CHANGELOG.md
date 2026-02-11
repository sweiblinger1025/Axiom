# PROJECT_CHANGELOG.md

## How to use
- Append a new session at the top (reverse chronological).
- Keep entries short and factual.
- Link to docs/paths when relevant.
- Record only *new* D-decisions here; DECISIONS.md is the source of truth.
- Tags: `[DOCS]` `[ABI]` `[A1]` `[A2]` `[B]` `[INFRA]` `[CSHARP]`

## Milestone Index (rolling)
- **Milestone 0 (Docs-First Restart):** ✅ All 8 docs locked
- **Milestone A1 (Shooting Range):** 🟨 In progress — ABI + core + 153 tests passing, save/load continuity test remaining
- **Milestone A2 (Combat Arena AI):** ⬜ Not started
- **Milestone B (RPG Slice):** ⬜ Not started

---

## 2026-02-10 — CMake Build System + MSVC Fix [BUILD][INFRA]

### Completed
- Created three-level CMake structure: root → engine/ → apps/headless/
- `axiom_core` builds as static library, `axiom_headless` links against it
- C++20 standard, extensions OFF (MSVC portability)
- Strict warnings on both GCC/Clang (-Wall -Wextra -Wpedantic) and MSVC (/W4)
- Fixed MSVC C2124 error: replaced `0.0f / 0.0f` with `NAN` macro in error path test
- Added `<cmath>` include for NAN
- Verified: 153/153 tests pass on both GCC 13.3.0 and MSVC (CLion + command line)

### Files
- `CMakeLists.txt` (root)
- `engine/CMakeLists.txt`
- `apps/headless/CMakeLists.txt`
- `apps/headless/main.cpp` (NAN fix only)

---

## 2026-02-10 — ABI Rebuild + Core Implementation + Headless Tests [ABI][A1]

### Completed
- Rebuilt `ax_abi.h` from scratch to match WORLD_INTERFACE.md v0.4 exactly
  - All 9 known divergences resolved (see 2026-02-08 Known Issues)
  - C11 enums, consistent 4-parameter buffer patterns, complete action structures
  - Added `AX_EVT_FIRE_BLOCKED` (COMBAT_A1 additive event)
- Implemented `ax_core.cpp` with full A1 gameplay logic
  - Lifecycle: create/destroy, content load/unload
  - Content system: weapon + target definitions with validation
  - World setup: player + target entity spawning
  - Movement: MOVE_INTENT with player-local transform, ground plane clamp
  - Look: LOOK_INTENT with delta yaw/pitch, pitch clamping
  - Fire: hitscan ray–sphere intersection, closest-hit selection, flat damage
  - Reload: tick-based countdown per COMBAT_A1 spec
  - Events: DAMAGE_DEALT, TARGET_DESTROY, RELOAD_STARTED, RELOAD_DONE, FIRE_BLOCKED
  - Snapshots: copy-out blob with header, entities, weapon state, events
  - Save/load: binary format matching SAVE_FORMAT.md v0.3 with checksum
  - Error handling: last-error strings, buffer-too-small pattern
  - Action validation: two-phase (structural at submit, state-dependent at tick)
- Built comprehensive headless test suite (153 tests)
  - `test_basic_fire_and_damage`: lifecycle, content, fire, damage, target destruction
  - `test_reload_cycle`: reload mechanics, tick countdown, ammo transfer, edge cases
  - `test_deterministic_replay`: identical action sequences → identical outcomes
  - `test_error_paths`: null params, bad versions, buffer too small, NaN rejection
- COMBAT_A1 acceptance criteria status:
  - ✅ #1 Headless run (scripted sequence, assert logical outcomes)
  - ✅ #2 Deterministic replay (identical state + actions → identical results)
  - 🟨 #3 Save/load continuity (functions exist, dedicated test not yet written)

### Files
- `engine/include/ax_abi.h` (complete ABI header)
- `engine/src/ax_core.cpp` (complete A1 core implementation)
- `apps/headless/main.cpp` (153-test harness)

### Known Issues (carry forward)
- Save/load continuity test (COMBAT_A1 acceptance #3) not yet implemented

---

## 2026-02-08 — Docs Lockdown + Repo Skeleton [DOCS][A1][ABI][INFRA]

### Completed
- Reviewed and locked all 8 foundation documents through iterative feedback cycles
- Established doc review workflow: draft → Claude review → revise → lock
- Created DECISIONS_ARCHIVE.md for 2D prototype decisions (D001–D013) with supersession rules
- Defined complete A1 combat contract (hitscan, reload, events, acceptance criteria)
- Created repo folder structure matching ARCHITECTURE.md module layout
- Built initial ABI stubs (compiles + runs, but diverges from locked docs — fixes queued)
- Established C# solution skeleton (empty csproj placeholders, gitignore for native/)
- Identified 9 divergences between stub code and locked WORLD_INTERFACE spec

### Documents
| Document | Status | Version |
|---|---|---|
| `docs/VISION.md` | LOCKED | v0.3 |
| `docs/DECISIONS.md` | ACTIVE | D100–D112 |
| `docs/DECISIONS_ARCHIVE.md` | ARCHIVE | D001–D013 |
| `docs/ARCHITECTURE.md` | LOCKED | v0.4 |
| `docs/WORLD_INTERFACE.md` | LOCKED | v0.4 |
| `docs/COMBAT_A1.md` | LOCKED | v0.4 |
| `docs/CONTENT_DATABASE.md` | LOCKED | v0.3 |
| `docs/SAVE_FORMAT.md` | LOCKED | v0.3 |

### Decisions Locked
- **D100** — Primary target: 3D Fallout-style RPG
- **D101** — Core library + separate apps (C ABI boundary)
- **D102** — Determinism tier policy (logic guaranteed, spatial stretch)
- **D103** — Truth/presentation split applies to 3D
- **D104** — Save versioning + migrations from day one
- **D105** — Windows-first (no portability work in v1)
- **D106** — Tick-based simulation core (rate TBD)
- **D107** — No engine-owned animation
- **D108** — ABI versioning uses MAJOR/MINOR
- **D109** — Snapshots use copy-out in v1
- **D110** — A1 hitscan uses truth look direction
- **D111** — Reload durations authored in ticks
- **D112** — A1 uses next-tick action scheduling

### Repo Structure Established
```
engine/
  include/          # ax_abi.h (C ABI header)
  src/
    abi/            # ABI boundary implementation
    content/        # ax_content (JSON loading)
    core/           # ax_core (lifecycle, tick stepping)
    debug/          # ax_debug (diagnostics)
    physics/        # ax_physics_iface (collision queries)
    save/           # ax_save (serialization)
    sim/            # ax_sim (rules, systems)
    world/          # ax_world (entity state)
apps/
  headless/         # C++ headless shell (A1 acceptance harness)
  viewer_csharp/    # C# solution (empty skeleton)
    src/
      Axiom.Bindings/
      Axiom.Viewer/
      Axiom.Tools/
    tests/
      Axiom.Bindings.Tests/
    native/win-x64/ # gitignored, DLL drop zone
tools/
  scripts/          # sync-native-to-csharp.ps1
docs/               # locked markdown docs
```

### Known Issues (resolved)
ABI header (`ax_abi.h`) had 9 divergences from WORLD_INTERFACE.md v0.4 — all resolved in 2026-02-10 session.