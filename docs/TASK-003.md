# TASK-003 — Debug Command Pipeline (Single-Cell Set)

## Goal

Introduce the first **command pipeline** into the engine, validating `COMMAND_MODEL.md`
end-to-end without introducing any simulation physics or numeric policy (D005 remains unresolved).

This task establishes:
- Command submission through the C ABI
- Tick-boundary **sequential** validation + application
- Deterministic command results visible to both C++ and C#
- First mutation of world state beyond creation

Scope is intentionally limited to **single-cell debug commands** only.

---

## Non-Goals

- No physics (thermal, gas, liquid)
- No fixed-point arithmetic (D005)
- No rectangle / batch commands
- No multiplayer arbitration
- No persistence / save files

---

## New Concepts Introduced

- **Command submission queue** (engine-owned)
- **Tick-boundary processing** (inside `World::step()`)
- **Command results buffer** (idempotent read, cleared on next tick)

---

## Command Model (v0)

### Validation: Structural vs State-Dependent

Per `COMMAND_MODEL.md`:

- **Structural validation happens at submission time** (cheap, no world-state dependency):
  - null handle / null pointer
  - unknown command type
  - version mismatch
  - malformed payload (v0: fixed-size command struct, so mostly version/type checks)

  Structural failures mean the command is **not queued**, and submission returns `0`.

- **State-dependent validation happens at tick processing time** (uses world state):
  - bounds checks
  - channel validity
  - other rules that depend on current state

  State-dependent failures produce a **result record** with `accepted = 0`.

### Processing Semantics

- Commands are submitted at any time and queued.
- Commands are processed **sequentially** at the next `step()` call.
- For each queued command, in submission order:
  1. Validate against *current* world state
  2. If valid, apply immediately
  3. Emit a result record

Validation logic is pure; mutation happens only on acceptance.

### Result Lifetime (No Drain Semantics)

- Results from tick *N* remain readable until `step()` advances to *N+1*.
- At the **start of each** `step()`, previous results are cleared.
- Reads are **idempotent** within a tick.

---

## C API Additions

### Command Types

Command type values are defined in the C header for readability. For ABI stability,
the **wire representation** in structs uses `uint32_t`.

```c
typedef enum ax_command_type {
    AX_CMD_NONE = 0,

    /* Debug-only commands (v0) */
    AX_CMD_DEBUG_SET_CELL_U8 = 1000
} ax_command_type;
```

### Reject Reasons (C-Compatible, Fixed Width)

C does not support `enum : uint8_t`. To guarantee ABI size, reject reasons are
defined as a `uint8_t` with named constants.

```c
typedef uint8_t ax_command_reject_reason;

#define AX_CMD_REJECT_NONE            ((ax_command_reject_reason)0)
#define AX_CMD_REJECT_INVALID_COORDS  ((ax_command_reject_reason)1)
#define AX_CMD_REJECT_INVALID_CHANNEL ((ax_command_reject_reason)2)
```

> IMPORTANT: Reject reasons are only for **state-dependent** failures discovered at tick processing time.
> Structural failures must return `0` from submission and produce **no** result record.

### Command Payload (v1)

Single-cell set for `uint8_t` grids (terrain, occupancy).

```c
typedef struct ax_cmd_set_cell_u8_v1 {
    uint32_t x;
    uint32_t y;
    uint8_t  channel; /* AX_SNAP_TERRAIN or AX_SNAP_OCCUPANCY */
    uint8_t  value;
    uint16_t _pad;
} ax_cmd_set_cell_u8_v1;
```

### Command Descriptor (v1)

```c
typedef struct ax_command_v1 {
    uint32_t version; /* must be 1 */

    /* Store as uint32_t for ABI stability across C/C++ compilers. */
    uint32_t type;    /* ax_command_type */

    union {
        ax_cmd_set_cell_u8_v1 setCellU8;
        uint8_t               _raw[32]; /* v0 payload cap */
    } payload;
} ax_command_v1;
```

Notes:
- The 32-byte payload cap is a **v0 constraint** and may be revised later.
- `payloadBytes` is intentionally omitted; payload shape is implied by `type + version`.

---

## Command Submission

```c
uint64_t ax_world_submit_command(
    ax_world_handle world,
    const ax_command_v1* cmd);
```

### Behavior

- Returns **engine-assigned command ID** (monotonic per world).
- Returns `0` for **structural failures** (not queued, no result):
  - `world` is null/invalid
  - `cmd` is null
  - `cmd->version != 1`
  - `cmd->type` is unknown / unsupported

Submission does **not** perform state-dependent checks (coords/channel). Those are deferred
to tick processing.

---

## Command Results

### Result Struct (v1)

To avoid hidden cross-compiler padding, fields are ordered and padded explicitly.
`type` is stored as `uint32_t` (wire format).

```c
typedef struct ax_command_result_v1 {
    uint32_t sizeBytes;   /* sizeof(ax_command_result_v1) */
    uint32_t version;     /* must be 1 */

    uint64_t commandId;
    uint64_t tickApplied;

    uint32_t type;        /* ax_command_type */
    uint8_t  accepted;    /* 1 = applied, 0 = rejected */
    uint8_t  reason;      /* ax_command_reject_reason */
    uint16_t _pad;        /* explicit alignment */
} ax_command_result_v1;
```

- `accepted = 1` → mutation applied
- `accepted = 0` → rejected, reason indicates state-dependent failure

### Reading Results

```c
uint32_t ax_world_read_command_results(
    ax_world_handle world,
    void* outBuffer,
    uint32_t outBufferSizeBytes);
```

Contract (matches snapshot semantics from TASK-001/TASK-002):
- `outBuffer == NULL` → returns required size
- undersized buffer → returns required size
- valid buffer → writes results, returns bytes written
- results remain valid until the next `step()`

Return value:
- `> 0` → required/written bytes
- `0` → error (null/invalid world, etc.)

---

## Validation Rules (State-Dependent)

For `AX_CMD_DEBUG_SET_CELL_U8`, during tick processing:

Reject if:
- `(x, y)` out of bounds → `accepted = 0`, `reason = AX_CMD_REJECT_INVALID_COORDS`
- `channel` not one of `{AX_SNAP_TERRAIN, AX_SNAP_OCCUPANCY}` → `accepted = 0`, `reason = AX_CMD_REJECT_INVALID_CHANNEL`

Value range (v0):
- Any `uint8_t` value is accepted for both channels (no semantic restrictions yet).

---

## Snapshot Interaction

- Accepted commands mutate SoA storage during the tick processing step.
- Snapshot reads reflect mutations after the tick completes.
- Rejected commands produce no mutation.

---

## Static Layout Guarantees

Engine and viewer must enforce byte-level consistency:

C/C++:
```c
static_assert(sizeof(ax_cmd_set_cell_u8_v1) == 12, "ax_cmd_set_cell_u8_v1 size mismatch");
static_assert(sizeof(ax_command_v1) == 40, "ax_command_v1 size mismatch");
static_assert(sizeof(ax_command_result_v1) == 32, "ax_command_result_v1 size mismatch");
```

C#:
- `[StructLayout(LayoutKind.Sequential, Pack = 1)]` is **not** required if we match natural alignment;
  instead validate actual size with `Marshal.SizeOf<T>()` against the expected values above.
- Use explicit field types (`uint`, `ulong`, `byte`, `ushort`) and verify sizes at runtime.

---

## Acceptance Criteria

### C++ (Headless)

- Submit valid set-cell command → non-zero commandId
- Step once → result accepted, tickApplied correct, snapshot reflects change
- Submit out-of-bounds coords → non-zero commandId, step → rejected with INVALID_COORDS, no mutation
- Submit invalid channel → non-zero commandId, step → rejected with INVALID_CHANNEL, no mutation
- Submit unknown type → submission returns `0`, no queued result
- Results readable until next `step()`
- Results cleared on subsequent `step()`
- Command IDs monotonic per world
- No crashes, no leaks

### C# (Viewer)

- Identical behavior and byte-level agreement with C++ side
- All struct sizes match expected (size asserts)
- Deterministic results across ABI

---

## Completion Notes

This task establishes the **command backbone** required by all future systems.
Subsequent tasks may add command types or batching, but the pipeline semantics remain consistent with `COMMAND_MODEL.md`:
- structural checks at submission
- state checks at tick boundary
- sequential validate→apply
- deterministic results
