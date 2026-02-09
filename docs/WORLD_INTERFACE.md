# WORLD_INTERFACE.md — C ABI, Actions, and Snapshots

**Version:** 0.4  
**Status:** LOCKED  
**Last Updated:** 2026-02-08  
**Depends On:** VISION.md v0.3 (LOCKED), DECISIONS.md (ACTIVE), ARCHITECTURE.md v0.4 (LOCKED)

---

## Purpose

Define the **concrete Core ↔ App boundary** for v1:
- the **C ABI surface** exposed by Axiom Core (D101)
- the **Action** submission format (typed, versioned, two-phase validation)
- the **Snapshot** format (minimum content for A1/A2/B)
- memory ownership + lifetime rules (so apps can be headless or fully rendered)

This doc exists so someone can implement:
- a **headless shell** for tests/CI
- a **3D viewer shell** for Milestone A1
without inventing interface rules.

---

## Scope

In scope for v1:
- ABI versioning and compatibility rules
- Core instance lifecycle + expected call order
- Action batch format and submission rules
- Snapshot shape and access rules
- Diagnostics/error reporting
- Save/load function signatures (format details elsewhere)

Out of scope:
- save file container details (see `SAVE_FORMAT.md`)
- content record schemas (see `CONTENT_DATABASE.md`)
- rendering, animation, asset pipelines

---

## Principles (non-negotiable)

- **C ABI only** across the boundary (no C++ types, no STL).
- **Two-phase action validation**:
  1) structural validation at submission (format/ranges/versioning)
  2) state-dependent validation at tick execution (world-dependent)
- **Logic determinism**: tick-stamped actions + identical initial state ⇒ identical truth transitions (D102).
- Snapshots are **read-only**; apps never mutate truth.

---

## Versioning

### ABI version

The 2D prototype used a single integer ABI version. v1 adopts an explicit **MAJOR/MINOR** split (see D108):
- `AX_ABI_MAJOR` — breaking changes (requires recompile of shells)
- `AX_ABI_MINOR` — additive changes (backward compatible within same MAJOR)

Core exposes:
- `ax_get_abi_version()` returning `{major, minor}`

### Struct versioning

Every ABI struct that crosses the boundary **independently** (function parameters and blob headers) includes:
- `uint16_t version`
- `uint16_t reserved` (padding / future use)
- `uint32_t size_bytes`

Sub-structs inside a versioned container (e.g., actions inside a batch, entities/events inside a snapshot) do **not** carry their own version headers in v1.

**Clarification:** `size_bytes` is for the **individual struct** (or blob header), not a cap on any whole-world save.
Large blobs (snapshots/saves) use `uint32_t` sizes and can exceed 64KB.

Rule: Core must reject unknown major struct versions at submission time.

---

## Error Model

All API calls return an `ax_result`:

- `AX_OK`
- `AX_ERR_INVALID_ARG`
- `AX_ERR_BAD_STATE`
- `AX_ERR_UNSUPPORTED`
- `AX_ERR_BUFFER_TOO_SMALL`
- `AX_ERR_PARSE_FAILED`
- `AX_ERR_IO`
- `AX_ERR_INTERNAL`

Core also exposes a best-effort last error string for debugging:

```c
const char* ax_get_last_error(void);
```

Notes:
- `ax_get_last_error()` is **not** stable gameplay data; it is for logs and diagnostics.
- It returns a pointer owned by Core; treat it as valid until the next Core call.

### Buffer-too-small rule (explicit)

For APIs that write into a caller-provided buffer:
- the `out_size_bytes` **must be written regardless of success/failure**
- if the buffer is too small, Core returns `AX_ERR_BUFFER_TOO_SMALL` and still writes the **required** size into `out_size_bytes`

This avoids mixed “return size / 0 on error” patterns from the 2D prototype.

---

## Threading Model (v1)

- v1 is **single-threaded simulation** by default.
- The app may call Core from **one thread** only.
- `ax_step_ticks()` is not re-entrant.
- Borrowed snapshot pointers (if used) are only valid until the next Core call that advances time or regenerates snapshot buffers.

If/when threading is introduced later, it must not break determinism.

---

## Stable ID Width (v1 lock)

For v1, entity IDs and record IDs are **32-bit unsigned** (`uint32_t`).

Rationale:
- 4,294,967,295 IDs is sufficient for v1 scopes (A1/A2/B).
- Changing this later is expensive, so we choose deliberately now.

---

## Expected Call Order (lifecycle sequence)

Typical shell lifecycle:

1) `ax_create(...)`
2) `ax_load_content(...)`
3) optional: `ax_load_save_bytes(...)`
4) main loop:
   - `ax_submit_actions(...)` (for tick N or “next tick”)
   - `ax_step_ticks(1)` (or `n_ticks` for headless fast-forward)
   - `ax_get_snapshot_*()` (render/log/test)
5) optional: `ax_save_bytes(...)`
6) `ax_unload_content(...)`
7) `ax_destroy(...)`

---

## Core Instance Lifecycle

### Types

```c
typedef struct ax_core ax_core;   // opaque handle
```

### Creation / destruction

```c
ax_result ax_create(const ax_create_params_v1* params, ax_core** out_core);
void      ax_destroy(ax_core* core);
```

`ax_create_params_v1` (v1 minimum):
- ABI version requested
- optional log callback (function pointer + user data)

Allocator hooks are explicitly **out of scope for v1** (use default malloc/free internally).

Example shape:

```c
typedef void (*ax_log_fn)(void* user, int level, const char* msg);

typedef struct ax_create_params_v1 {
    uint16_t version;       // = 1
    uint16_t reserved;
    uint32_t size_bytes;

    uint16_t abi_major;
    uint16_t abi_minor;

    ax_log_fn log_fn;       // optional
    void*     log_user;     // optional
} ax_create_params_v1;
```

---

## Content Loading (required in v1)

```c
ax_result ax_load_content(ax_core* core, const ax_content_load_params_v1* params);
ax_result ax_unload_content(ax_core* core);
```

`ax_content_load_params_v1` (stub; details in `CONTENT_DATABASE.md`):

```c
typedef struct ax_content_load_params_v1 {
    uint16_t version;       // = 1
    uint16_t reserved;
    uint32_t size_bytes;

    const char* root_path;  // path to a content directory or manifest
} ax_content_load_params_v1;
```

Notes:
- Content must be loaded before loading a save (ARCHITECTURE dependency rule).
- Content format/schema is defined in `CONTENT_DATABASE.md`.

---

## Save / Load (signatures belong here)

The shell owns IO. Core produces/consumes save bytes.

```c
ax_result ax_save_bytes(
    ax_core* core,
    uint32_t* out_size_bytes,
    void* out_buffer   // optional; if NULL, query required size
);

ax_result ax_load_save_bytes(
    ax_core* core,
    const void* data,
    uint32_t size_bytes
);
```

Format details live in `SAVE_FORMAT.md`.

---

## Diagnostics

```c
ax_result ax_get_diagnostics(ax_core* core, ax_diagnostics_v1* out_diag);
```

`ax_diagnostics_v1` should include:
- engine build hash / version string
- ABI major/minor
- feature flags (optional)
- current tick counter

---

## Actions

### Action submission API

```c
ax_result ax_submit_actions(ax_core* core, const ax_action_batch_v1* batch);
```

Rules:
- Actions are submitted **for a target tick** (absolute tick index) or “next tick”.
- Core applies actions deterministically in a defined order:
  1) sorted by `tick`
  2) stable submission order within the same tick (batch order)

State-dependent rejection (e.g., reload when no weapon) occurs at tick execution, not at submission.

### Action batch (v1)

Actions are fixed-size in v1; the element stride for the `actions` array is always `sizeof(ax_action_v1)` regardless of action type.

```c
typedef struct ax_action_batch_v1 {
    uint16_t version;        // = 1
    uint16_t reserved;
    uint32_t size_bytes;

    uint32_t count;
    const struct ax_action_v1* actions;   // count entries
} ax_action_batch_v1;
```

### Action header + tagged union (v1)

Per-action versioning is intentionally **not** included in v1 because actions are fixed-size and already governed by the batch `version`.
If/when variable-length actions are introduced, they can be added under a new batch version.

```c
typedef enum ax_action_type_v1 {
    AX_ACT_MOVE_INTENT   = 1,
    AX_ACT_LOOK_INTENT   = 2,
    AX_ACT_FIRE_ONCE     = 3,
    AX_ACT_RELOAD        = 4,
    AX_ACT_SPRINT_HELD   = 5,   // optional v1
    AX_ACT_CROUCH_TOGGLE = 6    // optional v1
} ax_action_type_v1;

typedef struct ax_action_v1 {
    uint64_t tick;           // target tick
    uint32_t actor_id;       // stable entity id (player in A1)
    uint32_t type;           // ax_action_type_v1
    union {
        struct { float x; float y; } move;             // 2D input vector
        struct { float yaw; float pitch; } look;       // delta vs absolute (TBD)
        struct { uint32_t weapon_slot; } fire_once;    // slot 0 in A1
        struct { uint32_t weapon_slot; } reload;
        struct { uint8_t held; uint8_t pad[3]; } sprint_held;
        struct { uint8_t unused; uint8_t pad[3]; } crouch_toggle;
    } u;
} ax_action_v1;
```

Notes:
- Floats in input are acceptable in v1 because **logic determinism** is defined at the Core transition level, not bit-identical spatial results.
- If float nondeterminism becomes a problem, actions can be quantized later (minor version bump if additive).

### Two-phase validation

Submission-time (structural) validation:
- batch `version` matches
- `size_bytes >= sizeof(ax_action_batch_v1)`
- `count` is reasonable
- action type tags are known
- numeric fields are finite (no NaNs/Infs)

Tick-time (state-dependent) validation:
- actor exists
- actor can perform action given current state
- action results in deterministic events/state transitions

---

## Snapshots

### Snapshot access API

v1 supports two patterns; pick one during implementation:

**A) Copy-out (simplest, safest):**
```c
ax_result ax_get_snapshot_bytes(
    ax_core* core,
    uint32_t* out_size_bytes,
    void* out_buffer   // optional; if NULL, query required size
);
```

**B) Borrowed immutable view (faster, more rules):**
```c
ax_result ax_get_snapshot_view(
    ax_core* core,
    const void** out_ptr,
    uint32_t* out_size_bytes
);
```

If B is used:
- the pointer is valid until the next `ax_step_ticks()` call
- the buffer must be treated as immutable by the app

### Snapshot events are per-tick (intent)

Snapshot events represent the events emitted for the **current tick** only.
The tick is therefore implicit from the snapshot header (no per-event tick field in v1).

If we later support multi-tick event buffers, events will gain an explicit tick field under a new version.

### Snapshot content (minimum v1)

Snapshots must contain enough state for A1/A2/B:

**Required fields (A1):**
- current tick counter
- player transform (position + rotation)
- target transforms (position + rotation)
- target HP values
- player weapon state: ammo + reload state
- event list for the tick (damage dealt, reload, target destroyed)

**Additional fields (A2/B):**
- AI actor transforms + health
- “space id” / active space indicator once multi-space exists

### Snapshot shape (v1 proposal)

A minimal “channels in one blob” approach:

- `ax_snapshot_header_v1`
- `ax_snapshot_entity_v1[]`
- `ax_snapshot_player_weapon_v1` (optional; present if player exists)
- `ax_snapshot_event_v1[]`

Blob layout (byte order):
1) `ax_snapshot_header_v1` at offset 0
2) entity array immediately after header (`entity_count * entity_stride_bytes`)
3) optional `ax_snapshot_player_weapon_v1` immediately after entities if `player_weapon_present == 1`
4) event array last (`event_count * event_stride_bytes`)


```c
typedef struct ax_snapshot_header_v1 {
    uint16_t version;         // = 1
    uint16_t reserved;
    uint32_t size_bytes;      // total blob size in bytes

    uint64_t tick;

    uint32_t entity_count;
    uint32_t entity_stride_bytes;   // = sizeof(ax_snapshot_entity_v1)

    uint32_t event_count;
    uint32_t event_stride_bytes;    // = sizeof(ax_snapshot_event_v1)

    uint32_t flags;           // reserved
    uint32_t player_weapon_present; // 0/1
} ax_snapshot_header_v1;

typedef struct ax_snapshot_entity_v1 {
    uint32_t id;
    uint32_t archetype_id;    // content record id (optional v1)

    float px, py, pz;
    float rx, ry, rz, rw;     // quaternion

    int32_t hp;               // -1 if not applicable
    uint32_t state_flags;     // e.g., is_player, is_target, is_dead
} ax_snapshot_entity_v1;

typedef struct ax_snapshot_player_weapon_v1 {
    uint32_t player_id;
    uint32_t weapon_slot;     // 0 in A1

    int32_t  ammo_in_mag;
    int32_t  ammo_reserve;

    uint32_t weapon_flags;    // e.g., reloading
    float    reload_progress; // 0..1 (or seconds remaining; TBD but consistent)
} ax_snapshot_player_weapon_v1;


typedef enum ax_event_type_v1 {
    AX_EVT_DAMAGE_DEALT   = 1,
    AX_EVT_RELOAD_STARTED = 2,
    AX_EVT_RELOAD_DONE    = 3,
    AX_EVT_TARGET_DESTROY = 4
} ax_event_type_v1;

typedef struct ax_snapshot_event_v1 {
    uint32_t type;        // ax_event_type_v1
    uint32_t a;           // attacker/actor id
    uint32_t b;           // target id
    int32_t  value;       // e.g., damage amount
} ax_snapshot_event_v1;
```

Note: `reload_progress` is snapshot truth exported for presentation. If stored internally as float, it must not affect logic determinism outcomes beyond the tick boundary policy (D102).

Quaternion note:
- rotation is represented as a quaternion; the app shell must interpret quaternions for rendering.

---

## Stepping the simulation

```c
ax_result ax_step_ticks(ax_core* core, uint32_t n_ticks);
```

Notes:
- Multi-tick stepping is supported for headless fast-forward and tests.
- Viewer shells may call with `n_ticks = 1` and render between ticks (interpolation strategy is an ARCHITECTURE Open Question).

---

## Open Questions

- Do we pick snapshot access A (copy-out) or B (borrowed view) for v1?
- Are look inputs deltas or absolute angles? (affects camera/controller integration)
- Do we quantize input actions in v1 for determinism friendliness, or keep floats?
- Do we include velocity in snapshots for render interpolation, or rely on dual snapshots?

---

## ChangeLog

### v0.1
- Initial draft: defines the v1 C ABI surface shape, versioned action model, two-phase validation, and snapshot minimum content for A1/A2/B.

### v0.2
- Adopted MAJOR/MINOR ABI versioning (explicit evolution from the 2D prototype’s single-integer scheme; see D108).
- Clarified `size_bytes` widths and semantics; large blobs use `uint32_t` sizes.
- Standardized buffer-too-small behavior: required size is always written to `out_size_bytes` even on error.
- Simplified action versioning: batch is versioned; per-action version/size removed for v1 fixed-size actions.
- Added lifecycle call sequence including content load/unload and optional save load.
- Added required v1 signatures for content load/unload and save/load (bytes).
- Resolved v1 stable ID width: `uint32_t` for entity and record IDs.
- Added weapon state to snapshots via `ax_snapshot_player_weapon_v1`.
- Added `ax_get_last_error()` signature and per-tick snapshot event intent note.
- Simplified `ax_create_params_v1`: allocator hooks explicitly out of scope for v1.

### v0.3
- Clarified which structs require version headers (function parameters and blob headers only).
- Stated `ax_action_v1` array stride is always `sizeof(ax_action_v1)`.
- Made snapshot blob byte layout explicit.
- Clarified `reload_progress` as snapshot truth and noted determinism implications.
- Added a stub definition for `ax_content_load_params_v1`.

### v0.4
- Moved `reload_progress` determinism note out of the code block for correct markdown rendering.
- Marked this document as LOCKED.
