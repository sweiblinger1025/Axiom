# CONTENT_DATABASE.md — v1 Content Records (JSON-First)

**Version:** 0.3  
**Status:** LOCKED  
**Last Updated:** 2026-02-08  
**Depends On:** ARCHITECTURE.md v0.4 (LOCKED), WORLD_INTERFACE.md v0.4 (LOCKED), COMBAT_A1.md v0.4 (LOCKED), DECISIONS.md (ACTIVE)

---

## Purpose

Define the **minimum v1 content system** required to implement **A1 Shooting Range** without an editor.

v1 content goals:
- stable IDs for records (referenced by saves)
- trivial loading from disk via `ax_load_content(...)`
- enough schema to define A1 weapon + A1 target archetype
- deterministic parsing and runtime representation (logic determinism tier)

Non-goals (v1):
- override layering / DLC / mods (explicitly v2+)
- hot-reload
- authoring tools / editor
- localization system
- complex validation beyond “fail fast”

---

## Core Rules (v1)

1) **Stable IDs are `uint32_t`.**  
   Content IDs must be consistent across sessions and machines.

2) **Content loads before save state.**  
   Saves reference content by stable ID; loading a save assumes the referenced content is available.

3) **Fail fast on missing required records.**  
   If required records are missing or malformed, `ax_load_content` fails with a non-OK result and a useful `ax_get_last_error()` message.

4) **JSON-first.**  
   v1 record format is JSON on disk. (Binary packing is out of scope until performance becomes a problem.)

---

## Loading API (ABI)

From `WORLD_INTERFACE.md`:

```c
ax_result ax_load_content(ax_core* core, const ax_content_load_params_v1* params);
ax_result ax_unload_content(ax_core* core);

typedef struct ax_content_load_params_v1 {
    uint16_t version;       // = 1
    uint16_t reserved;
    uint32_t size_bytes;

    const char* root_path;  // path to a content directory or manifest
} ax_content_load_params_v1;
```

v1 interpretation:
- `root_path` points to a **content root directory**.
- The root must contain a `manifest.json`.

---

## On-Disk Layout (v1)

```
content/
  manifest.json
  weapons/
    1000.json
  targets/
    2000.json
```

Notes:
- File names are the numeric ID for convenience. This is not a requirement, but it reduces ambiguity.
- `manifest.json` is the authoritative “what exists” index.

---

## manifest.json (v1)

### Shape

```json
{
  "version": 1,
  "weapons": [1000],
  "targets": [2000]
}
```

Note: `name` is **human-readable metadata only**; Core does not use it for lookups.

Rules:
- `version` is the manifest schema version (not the engine ABI version).
- Lists contain unique `uint32` IDs.
- Duplicate IDs (in the manifest list or in record files) **fail loading**.
- IDs listed must have corresponding files present.

---

## Record Types (v1)

### Weapon Record (A1)

File: `weapons/<id>.json`

```json
{
  "id": 1000,
  "type": "weapon",
  "name": "Range Rifle",

  "magazine_size": 12,
  "reload_duration_ticks": 30,
  "damage_per_hit": 10,

  "max_range_m": 75.0
}
```

Note: `name` is **human-readable metadata only**; Core does not use it for lookups.

Rules:
- `id` must match the filename ID and the manifest entry.
- `reload_duration_ticks` is integer ticks (D111).
- `max_range_m` is float truth used for hitscan distance checks (spatial-tier determinism is acceptable per D102).

Minimum validation:
- `magazine_size > 0`
- `reload_duration_ticks >= 1`
- `damage_per_hit >= 0`
- `max_range_m > 0`

Notes:
- Zero-damage weapons (`damage_per_hit == 0`) are valid and useful for testing hitscan and event plumbing without destroying targets.

### Target Record (A1)

File: `targets/<id>.json`

```json
{
  "id": 2000,
  "type": "target",
  "name": "Range Target",

  "max_hp": 50,
  "hit_sphere_radius_m": 0.35
}
```

Note: `name` is **human-readable metadata only**; Core does not use it for lookups.

Rules:
- `hit_sphere_radius_m` is used for ray–sphere intersection (COMBAT_A1 lock).
- `max_hp > 0`
- `hit_sphere_radius_m > 0`

---

## Runtime Representation (v1)

`ax_content` must provide lookup by ID:
- `weapon_def` by `weapon_id`
- `target_def` by `target_id`

v1 only needs:
- single weapon slot 0 for the player
- one or more targets sharing one target definition

No runtime mutation of content records.

---

## Determinism Notes

- JSON parsing must be deterministic for the same file contents.
- Numeric parsing must not depend on locale.
- Floating fields used in truth (e.g., `max_range_m`, `hit_sphere_radius_m`) are acceptable under D102’s spatial-tier best-effort policy.

---

## Error Handling

On failure, `ax_load_content` must:
- return a non-OK `ax_result`
- set `ax_get_last_error()` to a stable, actionable message (e.g., missing file, invalid field)

`ax_unload_content` must:
- always succeed (idempotent is acceptable) unless core is invalid

---

## Open Questions (v1)

- Should `root_path` also accept a direct path to `manifest.json` (vs directory only)?
- Do we want an optional “strict mode” for unknown JSON fields (ignore vs fail)?
- Do we want to precompute derived values (e.g., squared ranges/radii) for speed?

---

## ChangeLog

### v0.1
- Initial v1 content contract: JSON-first records with a simple manifest, stable `uint32` IDs, and minimal schemas for A1 weapon and A1 target.

### v0.2
- Tightened weapon validation: `reload_duration_ticks` must be >= 1.
- Clarified that zero-damage weapons are valid (test-friendly).
- Stated duplicate IDs fail loading.
- Clarified `name` fields are human-readable metadata only.

### v0.3
- Removed JSON comment from example (JSON has no comments) and moved the note to prose.
- Marked this document as LOCKED.
