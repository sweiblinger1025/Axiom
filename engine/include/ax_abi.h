/*
 * ax_abi.h — Axiom Core C ABI (v1)
 *
 * This header defines the stable C ABI boundary between Axiom Core
 * and all app shells (headless, viewer, tools).
 *
 * Rules:
 *   - Valid C11 (no C++ types, no STL)
 *   - Structs crossing the boundary independently carry version + size_bytes
 *   - Sub-structs inside versioned containers do NOT carry their own headers
 *
 * Authoritative spec: WORLD_INTERFACE.md v0.4 (LOCKED)
 */

#ifndef AX_ABI_H
#define AX_ABI_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Export / import macro ─────────────────────────────────────────── */

#if defined(AX_BUILD_SHARED)
  #if defined(_MSC_VER)
    #define AX_API __declspec(dllexport)
  #elif defined(__GNUC__) || defined(__clang__)
    #define AX_API __attribute__((visibility("default")))
  #else
    #define AX_API
  #endif
#elif defined(AX_IMPORT_SHARED)
  #if defined(_MSC_VER)
    #define AX_API __declspec(dllimport)
  #else
    #define AX_API
  #endif
#else
  #define AX_API
#endif

/* ── ABI version (D108) ───────────────────────────────────────────── */

#define AX_ABI_MAJOR 0
#define AX_ABI_MINOR 1

typedef struct ax_abi_version {
    uint16_t major;
    uint16_t minor;
} ax_abi_version;

AX_API ax_abi_version ax_get_abi_version(void);

/* ── Result codes ─────────────────────────────────────────────────── */

typedef enum ax_result {
    AX_OK                   = 0,
    AX_ERR_INVALID_ARG      = 1,
    AX_ERR_BAD_STATE        = 2,
    AX_ERR_UNSUPPORTED      = 3,
    AX_ERR_BUFFER_TOO_SMALL = 4,
    AX_ERR_PARSE_FAILED     = 5,
    AX_ERR_IO               = 6,
    AX_ERR_INTERNAL         = 7
} ax_result;

/* ── Last error (diagnostics only) ────────────────────────────────── */

AX_API const char* ax_get_last_error(void);

/* ── Opaque core handle ───────────────────────────────────────────── */

typedef struct ax_core ax_core;

/* ── Log callback ─────────────────────────────────────────────────── */

typedef void (*ax_log_fn)(void* user, int level, const char* msg);

/* ── Core creation parameters ─────────────────────────────────────── */

typedef struct ax_create_params_v1 {
    uint16_t version;       /* = 1                          */
    uint16_t reserved;      /* padding / future use         */
    uint32_t size_bytes;    /* sizeof(ax_create_params_v1)  */

    uint16_t abi_major;     /* ABI version shell expects    */
    uint16_t abi_minor;

    ax_log_fn log_fn;       /* optional (NULL = no logging) */
    void*     log_user;     /* optional (passed to log_fn)  */
} ax_create_params_v1;

/* ── Core lifecycle ───────────────────────────────────────────────── */

AX_API ax_result ax_create(const ax_create_params_v1* params, ax_core** out_core);
AX_API void      ax_destroy(ax_core* core);

/* ── Content loading ──────────────────────────────────────────────── */

typedef struct ax_content_load_params_v1 {
    uint16_t version;       /* = 1                              */
    uint16_t reserved;
    uint32_t size_bytes;    /* sizeof(ax_content_load_params_v1)*/

    const char* root_path;  /* path to content root directory   */
} ax_content_load_params_v1;

AX_API ax_result ax_load_content(ax_core* core, const ax_content_load_params_v1* params);
AX_API ax_result ax_unload_content(ax_core* core);

/* ── Save / Load (SAVE_FORMAT.md is source of truth) ──────────────── */

AX_API ax_result ax_save_bytes(
    ax_core*  core,
    void*     out_buf,          /* NULL = query required size    */
    uint32_t  out_cap_bytes,    /* capacity of out_buf           */
    uint32_t* out_size_bytes    /* always written: required size */
);

AX_API ax_result ax_load_save_bytes(
    ax_core*    core,
    const void* save_buf,
    uint32_t    save_size_bytes
);

/* ── Action types ─────────────────────────────────────────────────── */

typedef enum ax_action_type_v1 {
    AX_ACT_MOVE_INTENT   = 1,
    AX_ACT_LOOK_INTENT   = 2,
    AX_ACT_FIRE_ONCE     = 3,
    AX_ACT_RELOAD        = 4,
    AX_ACT_SPRINT_HELD   = 5,  /* optional v1 */
    AX_ACT_CROUCH_TOGGLE = 6   /* optional v1 */
} ax_action_type_v1;

/* ── Action (tagged union, fixed-size in v1) ──────────────────────── */

typedef struct ax_action_v1 {
    uint64_t tick;              /* target tick (absolute index)     */
    uint32_t actor_id;          /* stable entity id (player in A1)  */
    uint32_t type;              /* ax_action_type_v1                */
    union {
        struct { float x; float y; }           move;           /* 2D input vector      */
        struct { float yaw; float pitch; }     look;           /* delta yaw/pitch (A1) */
        struct { uint32_t weapon_slot; }       fire_once;      /* slot 0 in A1         */
        struct { uint32_t weapon_slot; }       reload;
        struct { uint8_t held; uint8_t pad[3]; }    sprint_held;
        struct { uint8_t unused; uint8_t pad[3]; }  crouch_toggle;
    } u;
} ax_action_v1;

/* ── Action batch ─────────────────────────────────────────────────── */

typedef struct ax_action_batch_v1 {
    uint16_t version;           /* = 1                              */
    uint16_t reserved;
    uint32_t size_bytes;        /* sizeof(ax_action_batch_v1)       */

    uint32_t count;
    const struct ax_action_v1* actions;  /* count entries            */
} ax_action_batch_v1;

AX_API ax_result ax_submit_actions(ax_core* core, const ax_action_batch_v1* batch);

/* ── Simulation stepping ──────────────────────────────────────────── */

AX_API ax_result ax_step_ticks(ax_core* core, uint32_t n_ticks);

/* ── Snapshot access (D109: copy-out only in v1) ──────────────────── */

AX_API ax_result ax_get_snapshot_bytes(
    ax_core*  core,
    void*     out_buf,          /* NULL = query required size    */
    uint32_t  out_cap_bytes,    /* capacity of out_buf           */
    uint32_t* out_size_bytes    /* always written: required size */
);

/* ── Snapshot blob layout ─────────────────────────────────────────── *
 *                                                                      *
 *   [ ax_snapshot_header_v1       ]                                    *
 *   [ ax_snapshot_entity_v1[]     ]  entity_count entries              *
 *   [ ax_snapshot_player_weapon_v1]  if player_weapon_present == 1     *
 *   [ ax_snapshot_event_v1[]      ]  event_count entries               *
 *                                                                      *
 * ──────────────────────────────────────────────────────────────────── */

typedef struct ax_snapshot_header_v1 {
    uint16_t version;               /* = 1                          */
    uint16_t reserved;
    uint32_t size_bytes;            /* total blob size in bytes     */

    uint64_t tick;

    uint32_t entity_count;
    uint32_t entity_stride_bytes;   /* = sizeof(ax_snapshot_entity_v1)        */

    uint32_t event_count;
    uint32_t event_stride_bytes;    /* = sizeof(ax_snapshot_event_v1)         */

    uint32_t flags;                 /* reserved                     */
    uint32_t player_weapon_present; /* 0 or 1                       */
} ax_snapshot_header_v1;

typedef struct ax_snapshot_entity_v1 {
    uint32_t id;
    uint32_t archetype_id;      /* content record id (0 if N/A) */

    float    px, py, pz;
    float    rx, ry, rz, rw;   /* quaternion                   */

    int32_t  hp;                /* -1 if not applicable         */
    uint32_t state_flags;       /* see AX_ENT_FLAG_* below      */
} ax_snapshot_entity_v1;

/* Entity state flags (bitmask for ax_snapshot_entity_v1.state_flags) */
#define AX_ENT_FLAG_PLAYER    (1u << 0)
#define AX_ENT_FLAG_TARGET    (1u << 1)
#define AX_ENT_FLAG_DEAD      (1u << 2)

typedef struct ax_snapshot_player_weapon_v1 {
    uint32_t player_id;
    uint32_t weapon_slot;       /* 0 in A1                      */

    int32_t  ammo_in_mag;
    int32_t  ammo_reserve;

    uint32_t weapon_flags;      /* see AX_WPN_FLAG_* below      */
    float    reload_progress;   /* 0.0 .. 1.0 for presentation  */
} ax_snapshot_player_weapon_v1;

/* Weapon state flags (bitmask for ax_snapshot_player_weapon_v1.weapon_flags) */
#define AX_WPN_FLAG_RELOADING (1u << 0)

/* ── Event types ──────────────────────────────────────────────────── */

typedef enum ax_event_type_v1 {
    AX_EVT_DAMAGE_DEALT   = 1,
    AX_EVT_RELOAD_STARTED = 2,
    AX_EVT_RELOAD_DONE    = 3,
    AX_EVT_TARGET_DESTROY = 4,
    AX_EVT_FIRE_BLOCKED   = 5   /* A1 additive (COMBAT_A1.md)   */
} ax_event_type_v1;

/* Fire-blocked reason codes (ax_snapshot_event_v1.value for FIRE_BLOCKED) */
#define AX_FIRE_BLOCKED_RELOADING  1
#define AX_FIRE_BLOCKED_EMPTY_MAG  2

typedef struct ax_snapshot_event_v1 {
    uint32_t type;      /* ax_event_type_v1             */
    uint32_t a;         /* attacker / actor id          */
    uint32_t b;         /* target id / weapon slot      */
    int32_t  value;     /* damage amount / reason code  */
} ax_snapshot_event_v1;

/* ── Diagnostics ──────────────────────────────────────────────────── */

#define AX_BUILD_HASH_LEN     32
#define AX_VERSION_STRING_LEN 64

typedef struct ax_diagnostics_v1 {
    uint16_t version;           /* = 1                              */
    uint16_t reserved;
    uint32_t size_bytes;        /* sizeof(ax_diagnostics_v1)        */

    uint16_t abi_major;
    uint16_t abi_minor;

    uint64_t current_tick;

    uint32_t feature_flags;     /* reserved (0 in v1)               */
    uint32_t pad0;              /* alignment                        */

    char     build_hash[AX_BUILD_HASH_LEN];         /* null-terminated */
    char     version_string[AX_VERSION_STRING_LEN];  /* null-terminated */
} ax_diagnostics_v1;

AX_API ax_result ax_get_diagnostics(ax_core* core, ax_diagnostics_v1* out_diag);

/* ── End of header ────────────────────────────────────────────────── */

#ifdef __cplusplus
}
#endif

#endif /* AX_ABI_H */