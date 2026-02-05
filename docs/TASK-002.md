# TASK-002 — Spatial Grid Allocation & Terrain / Occupancy Snapshots

## Purpose

Introduce the **spatial grid backbone** of the simulation:
- Allocate a 2D grid based on world dimensions
- Store terrain and occupancy as **Structure-of-Arrays (SoA)** fields
- Expose terrain and occupancy through the **existing snapshot API**
- Validate indexing, memory layout, and snapshot round-trips

This task deliberately avoids any numeric policy decisions (D005) and any simulation logic.

---

## Scope

### In Scope

- Allocate per-cell SoA arrays:
  - `terrain` (uint8)
  - `occupancy` (uint8)
- Arrays sized exactly `width * height`
- Default-initialize all cells to zero
- Add new snapshot channels:
  - `AX_SNAP_TERRAIN`
  - `AX_SNAP_OCCUPANCY`
- Read terrain and occupancy via:
  ```c
  uint32_t ax_world_read_snapshot(
      ax_world_handle world,
      ax_snapshot_channel channel,
      void* outBuffer,
      uint32_t outBufferSizeBytes);
  ```
- Preserve TASK-001 buffer semantics:
  - `outBuffer == NULL` returns the required size
  - undersized buffers also return the required size
  - `0` is reserved for true errors (invalid handle, unknown channel, null args where required)
- Add one new query:
  ```c
  uint32_t ax_world_get_cell_count(ax_world_handle world);
  ```

### Out of Scope (Explicit Non-Goals)

- Temperature, mass, pressure, or energy fields
- Fixed-point arithmetic (D005)
- Commands or mutation APIs
- Rendering, UI, or visualization
- Snapshot delta compression
- Channel metadata infrastructure (acknowledged but not implemented)

---

## API Additions

### Snapshot Channels

Extend the existing enum:
```c
typedef enum ax_snapshot_channel {
    AX_SNAP_WORLD_META = 1,
    AX_SNAP_TERRAIN    = 2,
    AX_SNAP_OCCUPANCY  = 3,
} ax_snapshot_channel;
```

No new snapshot functions are introduced.

### Cell Count Query

```c
uint32_t ax_world_get_cell_count(ax_world_handle world);
```

- Returns `width * height`
- Returns `0` if:
  - `world` is null
  - multiplication overflows `uint32_t`

---

## Snapshot Buffer Contract (Alignment with TASK-001)

The snapshot read contract is unchanged:

| Case | Behavior |
|---|---|
| `outBuffer == NULL` | Return required size in bytes |
| `outBufferSizeBytes < required` | Return required size in bytes |
| `outBufferSizeBytes >= required` | Fill buffer, return bytes written |
| Invalid world or channel | Return `0` |

**Undersized buffers MUST return the required size**, not 0.

---

## Data Layout

- One byte per cell
- Linear index:
  ```c
  index = y * width + x;
  ```
- Snapshot data is a flat array:
  - Length = `width * height`
  - Row-major order, bottom-to-top

---

## Validation & Acceptance Criteria

### Headless (C++)

- Create world with known dimensions
- Verify:
  - `ax_world_get_cell_count() == width * height`
- For both terrain and occupancy:
  - NULL buffer returns required size
  - Undersized buffer returns required size
  - Full buffer read succeeds
  - All values are zero
- Destroy world cleanly

### Viewer (C#)

- Mirror all C++ checks via P/Invoke
- Validate managed buffer receives correct data
- Confirm no crashes or memory corruption

---

## Notes on Channel Metadata

Per `SNAPSHOT_EVENT_FORMATS.md`, channels should eventually expose metadata
(unit, semantic meaning, quantity type).  
For this task:
- Terrain and occupancy are **unitless enum fields**
- No metadata plumbing is required yet
- This task simply acknowledges the requirement exists

---

## Completion Criteria

TASK-002 is complete when:
- Terrain and occupancy grids allocate correctly
- Snapshot reads round-trip correctly on both sides
- Undersized buffer semantics match TASK-001
- No new ABI patterns are introduced
- All tests pass in both headless and viewer

---

## Follow-On Tasks

- **TASK-003**: Spatial indexing helpers & neighbor queries
- **TASK-004**: D005 — Canonical fixed-point numeric policy
- **TASK-005**: First simulation system (thermal or gas)
