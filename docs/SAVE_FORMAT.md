# SAVE_FORMAT.md — v1 Save Bytes (A1 Minimum)

**Version:** 0.3  
**Status:** LOCKED  
**Last Updated:** 2026-02-08  
**Depends On:** ARCHITECTURE.md v0.4 (LOCKED), WORLD_INTERFACE.md v0.4 (LOCKED), COMBAT_A1.md v0.4 (LOCKED), DECISIONS.md (ACTIVE)

---

## Purpose

Define the **minimum v1 save/load format** required for:
- Milestone **A1: Shooting Range**
- logic-deterministic replay tests
- versioning/migrations from day one (D104)

This doc defines **what the save bytes contain**, how they are versioned, and the rules for loading.

Non-goals (v1):
- world streaming / unloaded cell deltas (Milestone B+)
- compression/encryption
- mod/DLC override layering behavior
- “partial saves” of specific systems

---

## Save/Load API (ABI)

From `WORLD_INTERFACE.md`:

```c
// NOTE: This refines the stub shown in WORLD_INTERFACE.md v0.4.
// SAVE_FORMAT is the source of truth for save/load ABI surface.
ax_result ax_save_bytes(ax_core* core, void* out_buf, uint32_t out_cap_bytes, uint32_t* out_size_bytes);
ax_result ax_load_save_bytes(ax_core* core, const void* save_buf, uint32_t save_size_bytes);
```

Rules:
- `ax_save_bytes` is “size query then copy”:
  - if `out_buf == NULL`, Core writes required size to `out_size_bytes` and returns `AX_OK`
  - if `out_cap_bytes < required`, Core writes required size to `out_size_bytes` and returns `AX_ERR_BUFFER_TOO_SMALL`
- `ax_load_save_bytes` validates header + version and either:
  - loads successfully, or
  - fails fast with `AX_ERR_INVALID_ARGUMENT` / `AX_ERR_IO` / `AX_ERR_UNSUPPORTED` and a useful last error message.

---

## Dependency Rule: Content First

**Content must be loaded before loading a save.**  
Saves reference content by stable `uint32` IDs (CONTENT_DATABASE v1).

If referenced content is missing:
- `ax_load_save_bytes` fails with `AX_ERR_INVALID_ARGUMENT`
- `ax_get_last_error()` explains which content ID/type is missing

---

## Encoding Rules (v1)

- **All multi-byte values are little-endian** in save bytes.
- Struct layouts are packed as written here (no implicit padding assumptions across languages; offsets are explicit where needed).

## High-Level Format

Save bytes are a single contiguous blob:

```
[ SaveHeaderV1 ][ A1WorldV1 ][ TargetsV1[] ]
```

v1 supports **A1 only**. Additional chunks (A2/B) are future versions.

---

## SaveHeaderV1

All integers are little-endian.

```c
typedef struct ax_save_header_v1 {
    uint32_t magic;        // 'AXSV' = 0x56535841
    uint16_t version_major; // = 1
    uint16_t version_minor; // = 0
    uint32_t total_size_bytes;

    uint32_t world_chunk_offset;
    uint32_t world_chunk_size_bytes;

    uint32_t checksum32;   // simple checksum over bytes excluding this field (v1)
} ax_save_header_v1;
```

Rules:
- `total_size_bytes` must match the provided `save_size_bytes`.
- `checksum32` is optional but recommended in v1 to catch truncation/corruption.
- Checksum byte range (v1): compute over `save_bytes[0 .. total_size_bytes-1]` treating the `checksum32` field itself as zero (i.e., write 0 to that 4-byte field during checksum computation). This ensures one unambiguous algorithm across implementations.
- Versioning uses major/minor:
  - major bumps are breaking without migrations
  - minor bumps are backward compatible if migration is supported

Migration policy:
- If `version_major == 1` and `version_minor <= current_minor`, Core migrates in-memory as needed.
- If `version_major > 1`, Core returns `AX_ERR_UNSUPPORTED`.

---

## A1WorldV1

```c
typedef struct ax_save_a1_world_v1 {
    uint64_t tick;

    // content references
    uint32_t weapon_id_slot0;    // from content database
    uint32_t target_def_id;      // from content database (A1 may assume one def shared by all targets)

    // player truth
    float px, py, pz;
    float rx, ry, rz, rw;        // quaternion

    // weapon truth (A1)
    int32_t ammo_in_mag;
    int32_t ammo_reserve;
    uint32_t reload_ticks_remaining; // 0 if not reloading
    // Note: `reloading` is derived as `reload_ticks_remaining > 0`.

    // target list
    uint32_t target_count;
    uint32_t targets_offset_bytes;   // absolute offset from start of blob
} ax_save_a1_world_v1;
```

Notes:
- Floats are acceptable under spatial-tier determinism (D102). A1 logic tests should not depend on float exactness beyond the defined outcomes.
- `targets_offset_bytes` allows future extension without tightly packing assumptions.

---

## TargetsV1

Packed array at `targets_offset_bytes`:

```c
typedef struct ax_save_target_v1 {
    uint32_t entity_id;

    float px, py, pz;
    float rx, ry, rz, rw;

    int32_t hp;
    uint32_t flags; // bit0 = destroyed
} ax_save_target_v1;
```

Rules:
- `entity_id` must be stable within the save.
- `hp` is preserved even if destroyed (for debugging).

---

## Save/Load Invariants (A1)

The following must hold:

1) **Snapshot equality at save tick**  
Immediately after loading a save created at tick T, the first snapshot produced by Core at tick T must match the snapshot at the moment of saving for A1 fields:
- player transform
- target transforms
- target HP + destroyed flags
- ammo and reload state

2) **Replay equivalence**  
If you save at tick T, then load and continue applying the same remaining actions, outcomes must match an uninterrupted run:
- ammo counts
- total damage from events
- target destruction set

---

## Error Handling

Load failures must:
- be non-destructive (Core remains valid; previous world remains unless explicitly reset)
- report a stable last error string

---

## Open Questions (v1)

- Do we want a stronger checksum (CRC32) vs a simple additive checksum?
- Should A1 saves include a “content manifest hash” to detect mismatched content?
- Do we want explicit alignment/padding rules for future binary stability?

---

## ChangeLog

### v0.1
- Initial v1 save contract for A1: a single blob with a versioned header, tick + player/weapon truth, and a packed target array, plus explicit invariants for snapshot equality and replay equivalence.

### v0.2
- Declared SAVE_FORMAT as the source of truth for save/load ABI; noted refinement vs WORLD_INTERFACE stub.
- Made tick width `uint64_t` to match snapshots.
- Clarified `reloading` derives from `reload_ticks_remaining > 0`.
- Fixed blob layout diagram (player embedded in A1World).
- Added top-level little-endian encoding rule.
- Made checksum algorithm explicit by zeroing the checksum field during computation.

### v0.3
- Added missing section separator before Encoding Rules for formatting consistency.
- Marked this document as LOCKED.
