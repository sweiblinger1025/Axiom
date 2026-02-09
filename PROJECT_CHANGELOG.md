# PROJECT_CHANGELOG.md

## How to use
- Append a new session at the top (reverse chronological).
- Keep entries short and factual.
- Link to docs/paths when relevant.
- Record only *new* D-decisions here; DECISIONS.md is the source of truth.
- Tags: `[DOCS]` `[ABI]` `[A1]` `[A2]` `[B]` `[INFRA]` `[CSHARP]`

## Milestone Index (rolling)
- **Milestone 0 (Docs-First Restart):** ✅ All 8 docs locked
- **Milestone A1 (Shooting Range):** 🟨 In progress — repo skeleton committed, ABI header next
- **Milestone A2 (Combat Arena AI):** ⬜ Not started
- **Milestone B (RPG Slice):** ⬜ Not started

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

### Known Issues (carry forward)
ABI header (`ax_abi.h`) has 9 divergences from WORLD_INTERFACE.md v0.4:
1. C++ typed enums (`: uint16_t`) — must be plain C11 enums
2. `ax_action_v1` missing `tick` + `actor_id` fields
3. `ax_action_batch_v1` has extra `stride` field + `void*` instead of typed pointer
4. Function named `ax_submit_actions_next_tick` — should be `ax_submit_actions`
5. `ax_snapshot_event_v1` missing `value` field (has 3 fields, spec has 4)
6. `ax_snapshot_entity_v1` uses `kind` instead of `archetype_id` + `state_flags`
7. `ax_create_params_v1` missing `abi_major` / `abi_minor` fields
8. Entity creation hardcoded in `ax_create` (should be post-content-load)
9. Empty `ax_last_error.cpp` included in build

### Next
- Fix `ax_abi.h` section-by-section to match WORLD_INTERFACE (new session)
- Update `ax_core.cpp` to match corrected header
- Update `apps/headless/main.cpp` to exercise corrected ABI
- Split CMakeLists.txt into root + `apps/headless/CMakeLists.txt`
- Begin A1 implementation: content loading → entity spawning → tick stepping → hitscan
