# TASK-001 — World Lifecycle + Tick Loop + Minimal Observation

## Goal
Implement the first real engine runtime object (a **World**) with a deterministic tick loop, exposed through the C API and callable from:

- `apps/headless` (C++ runner)
- `apps/viewer` (C# viewer via P/Invoke)

This task establishes:
- Handle-based lifetime management
- Tick advancement and tick counter
- Minimal snapshots plumbing (no simulation systems yet)

## Governing Docs
This task **must conform** to the following documents. If there is a conflict, these documents win:

- ARCHITECTURE_OVERVIEW.md
- TRUTH_VS_PRESENTATION.md
- COMMAND_MODEL.md
- SPATIAL_MODEL.md
- SNAPSHOT_EVENT_FORMATS.md
- TOOLCHAIN.md

---

## Scope

### In Scope
1. **C++ World object**
   - Owns authoritative state:
     - world dimensions (`width`, `height`)
     - `tick` counter (`uint64_t`)
   - Deterministic tick advancement
   - No physics systems yet

2. **C API: world lifecycle**
   - Create world from descriptor
   - Destroy world
   - Step world by N ticks
   - Query current tick
   - Query world dimensions

3. **Minimal snapshot mechanism**
   - One snapshot channel: world meta (tick + size)
   - Caller-allocated buffer pattern
   - Versioned snapshot struct

4. **Headless validation**
   - Create, step, snapshot, destroy
   - Validate all values

5. **C# viewer validation**
   - Same operations via P/Invoke
   - Print PASS/FAIL results

### Explicit Non‑Goals
- No commands
- No grid allocation
- No chunking
- No events
- No save/load
- No threading

---

## Repository Layout

```
engine/
  core/
  bindings/
    c/
      include/ax_api.h
      src/ax_api.cpp
apps/
  headless/
    main.cpp
  viewer/
    AxiomViewer/
      NativeBindings.cs
      Program.cs
```

---

## C API Additions

### Types

```c
typedef struct ax_world_t ax_world_t;
typedef ax_world_t* ax_world_handle;

typedef struct ax_world_desc
{
    uint32_t width;
    uint32_t height;
    uint32_t reserved;
} ax_world_desc;
```

### Lifecycle Functions

```c
AX_API ax_world_handle ax_world_create(const ax_world_desc* desc);
AX_API void ax_world_destroy(ax_world_handle world);

AX_API void ax_world_step(ax_world_handle world, uint32_t ticks);
AX_API uint64_t ax_world_get_tick(ax_world_handle world);
AX_API void ax_world_get_size(ax_world_handle world, uint32_t* outWidth, uint32_t* outHeight);
```

Rules:
- `ax_world_create(NULL)` → NULL
- Zero width or height → NULL
- Destroy/step on NULL → no‑op
- Step with `ticks == 0` → no‑op

---

## Snapshot API (Minimal)

### Channel

```c
typedef enum ax_snapshot_channel
{
    AX_SNAP_WORLD_META = 1
} ax_snapshot_channel;
```

### Snapshot Struct

```c
typedef struct ax_world_meta_snapshot_v1
{
    uint32_t version;
    uint32_t sizeBytes;
    uint64_t tick;
    uint32_t width;
    uint32_t height;
    uint32_t reserved;
} ax_world_meta_snapshot_v1;
```

### Snapshot Read

```c
AX_API uint32_t ax_world_read_snapshot(
    ax_world_handle world,
    ax_snapshot_channel channel,
    void* outBuffer,
    uint32_t outBufferSizeBytes);
```

Semantics:
- Returns required size if buffer is NULL or too small
- Writes full struct only if buffer is sufficient
- Snapshot reflects current world state

---

## Headless Validation Steps

1. Create world (64×48)
2. Assert tick == 0
3. Step 1 → tick == 1
4. Step 9 → tick == 10
5. Snapshot read:
   - query size
   - read struct
   - validate version, size, tick, width, height
6. Destroy world
7. Exit cleanly

---

## C# Viewer Validation

Repeat the same validation via P/Invoke:
- Use `IntPtr` for handles
- `uint` / `ulong` for sizes and ticks
- Snapshot read via `byte[]` or unmanaged buffer

---

## Acceptance Criteria

- World lifecycle works from both C++ and C#
- Tick counter is deterministic and correct
- Snapshot contract is respected
- No crashes or leaks
- No extra exported symbols
- TOOLCHAIN.md updated with TASK‑001 completion note

---

## Deliverables

- Updated `ax_api.h` / `ax_api.cpp`
- New C++ World implementation
- Updated headless app
- Updated C# viewer bindings
- Documentation update
