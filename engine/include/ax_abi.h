#pragma once
// Axiom C ABI — v1 stubs for A1 implementation
// This header is designed for C and foreign-function interfaces (C#, etc).

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------
// ABI Versioning (D108)
// -----------------------------

#define AX_ABI_MAJOR 1
#define AX_ABI_MINOR 0

typedef struct ax_abi_version {
    uint16_t major;
    uint16_t minor;
} ax_abi_version;

ax_abi_version ax_get_abi_version(void);

// -----------------------------
// Result Codes
// -----------------------------

typedef enum ax_result {
    AX_OK = 0,
    AX_ERR_INVALID_ARGUMENT = 1,
    AX_ERR_UNSUPPORTED = 2,
    AX_ERR_BUFFER_TOO_SMALL = 3,
    AX_ERR_INTERNAL = 4,
    AX_ERR_IO = 5
} ax_result;

// Opaque core handle
typedef struct ax_core ax_core;

// -----------------------------
// Create / Destroy
// -----------------------------

typedef void (*ax_log_fn)(void* user, int level, const char* msg);

typedef struct ax_create_params_v1 {
    uint16_t version;       // = 1
    uint16_t reserved;
    uint32_t size_bytes;

    ax_log_fn log_fn;       // optional
    void* log_user;         // optional
} ax_create_params_v1;

ax_result ax_create(const ax_create_params_v1* params, ax_core** out_core);
void ax_destroy(ax_core* core);

// -----------------------------
// Last Error String
// -----------------------------
// Thread-safety: v1 guarantees last-error per core instance, not globally.
// Returned pointer remains valid until next API call that mutates last error.

const char* ax_get_last_error(ax_core* core);

// -----------------------------
// Actions (A1 minimal shape)
// -----------------------------

typedef enum ax_action_type_v1 : uint16_t {
    AX_ACT_NONE = 0,
    AX_ACT_MOVE_INTENT = 1,   // (x,y) in [-1,1]
    AX_ACT_LOOK_INTENT = 2,   // delta yaw/pitch in radians (float)
    AX_ACT_FIRE_ONCE = 3,     // weapon_slot
    AX_ACT_RELOAD = 4,        // weapon_slot
    AX_ACT_SPRINT_HELD = 5,   // held
    AX_ACT_CROUCH_TOGGLE = 6  // toggle
} ax_action_type_v1;

// Fixed-stride action struct. Stride is always sizeof(ax_action_v1).
// See WORLD_INTERFACE.md v0.4 for contract. This is a stub representation.

typedef struct ax_action_v1 {
    uint16_t type;           // ax_action_type_v1
    uint16_t reserved0;
    union {
        struct { float x, y; } move_intent;
        struct { float delta_yaw, delta_pitch; } look_intent;
        struct { uint32_t weapon_slot; } fire_once;
        struct { uint32_t weapon_slot; } reload;
        struct { uint32_t held; } sprint_held;
        struct { uint32_t toggle; } crouch_toggle;
    } u;
} ax_action_v1;

// Batch header (boundary struct => version + size_bytes)
typedef struct ax_action_batch_v1 {
    uint16_t version;        // = 1
    uint16_t reserved;
    uint32_t size_bytes;

    uint32_t action_count;
    uint32_t action_stride_bytes; // must be sizeof(ax_action_v1)
    const void* actions;          // points to action_count * stride bytes
} ax_action_batch_v1;

// v1 scheduling rule: batch is applied on the NEXT tick only (D112).
ax_result ax_submit_actions_next_tick(ax_core* core, const ax_action_batch_v1* batch);

// -----------------------------
// Simulation stepping
// -----------------------------

ax_result ax_step_ticks(ax_core* core, uint32_t n_ticks);

// -----------------------------
// Snapshot (copy-out first, D109)
// -----------------------------

typedef struct ax_snapshot_header_v1 {
    uint32_t magic;           // 'AXSN' = 0x4E534841
    uint16_t version;         // = 1
    uint16_t reserved;
    uint32_t size_bytes;      // total blob size in bytes

    uint64_t tick;

    uint32_t entity_count;
    uint32_t entity_stride_bytes;

    uint32_t event_count;
    uint32_t event_stride_bytes;

    uint32_t weapon_present;  // 0/1
    uint32_t weapon_stride_bytes;
} ax_snapshot_header_v1;

// Minimal entity snapshot for A1 (player + targets)
typedef struct ax_snapshot_entity_v1 {
    uint32_t id;
    uint32_t kind;            // 0=player, 1=target
    float px, py, pz;
    float rx, ry, rz, rw;     // quat
    int32_t hp;
    uint32_t flags;           // bit0 destroyed
} ax_snapshot_entity_v1;

typedef struct ax_snapshot_player_weapon_v1 {
    uint32_t weapon_id;
    int32_t ammo_in_mag;
    int32_t ammo_reserve;
    uint32_t reload_ticks_remaining;
    float reload_progress;    // presentation-derived (may be float); see docs
} ax_snapshot_player_weapon_v1;

typedef enum ax_event_type_v1 : uint16_t {
    AX_EVT_NONE = 0,
    AX_EVT_DAMAGE = 1,
    AX_EVT_RELOAD_STARTED = 2,
    AX_EVT_RELOAD_COMPLETE = 3,
    AX_EVT_TARGET_DESTROYED = 4,
    // A1 additive event (COMBAT_A1): must be added to WORLD_INTERFACE implementation enum
    AX_EVT_FIRE_BLOCKED = 5
} ax_event_type_v1;

typedef struct ax_snapshot_event_v1 {
    uint16_t type;           // ax_event_type_v1
    uint16_t reserved;
    uint32_t a;              // meaning depends on event
    int32_t  b;              // meaning depends on event
} ax_snapshot_event_v1;

// Copy-out snapshot bytes.
// If out_buf == NULL, writes required size to out_size_bytes and returns AX_OK.
// If out_cap_bytes < required, writes required size and returns AX_ERR_BUFFER_TOO_SMALL.
ax_result ax_get_snapshot_bytes(ax_core* core, void* out_buf, uint32_t out_cap_bytes, uint32_t* out_size_bytes);

// -----------------------------
// Content loading (v1 JSON-first)
// -----------------------------

typedef struct ax_content_load_params_v1 {
    uint16_t version;       // = 1
    uint16_t reserved;
    uint32_t size_bytes;

    const char* root_path;  // directory containing manifest.json
} ax_content_load_params_v1;

ax_result ax_load_content(ax_core* core, const ax_content_load_params_v1* params);
ax_result ax_unload_content(ax_core* core);

// -----------------------------
// Save bytes (SAVE_FORMAT is source of truth for this ABI)
// -----------------------------

ax_result ax_save_bytes(ax_core* core, void* out_buf, uint32_t out_cap_bytes, uint32_t* out_size_bytes);
ax_result ax_load_save_bytes(ax_core* core, const void* save_buf, uint32_t save_size_bytes);

#ifdef __cplusplus
} // extern "C"
#endif
