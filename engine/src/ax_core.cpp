/*
 * ax_core.cpp — Axiom Core implementation (v1)
 *
 * This file implements the C ABI surface defined in ax_abi.h.
 * Internally C++; externally pure C via extern "C" linkage.
 *
 * Authoritative specs:
 *   WORLD_INTERFACE.md v0.4  — ABI surface + lifecycle
 *   COMBAT_A1.md v0.4        — gameplay rules (when implemented)
 *   SAVE_FORMAT.md v0.3      — save/load byte layout
 *   CONTENT_DATABASE.md v0.3 — content loading
 */

#include "ax_abi.h"

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <vector>
#include <cmath>

/* ── Last error (global, since ax_get_last_error takes void) ──────── */

static char g_last_error[256] = "";

static void set_last_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_last_error, sizeof(g_last_error), fmt, args);
    va_end(args);
}

/* ── Lifecycle state ──────────────────────────────────────────────── */

enum ax_lifecycle {
    AX_LIFECYCLE_CREATED,           /* after ax_create               */
    AX_LIFECYCLE_CONTENT_LOADED,    /* after ax_load_content         */
    AX_LIFECYCLE_RUNNING            /* after first ax_step_ticks     */
};

/* ── Internal entity (truth state) ────────────────────────────────── */

struct ax_entity_internal {
    uint32_t id;
    uint32_t archetype_id;

    float px, py, pz;
    float rx, ry, rz, rw;      /* quaternion                    */

    int32_t  hp;                /* -1 if not applicable          */
    uint32_t state_flags;       /* AX_ENT_FLAG_*                 */
};

/* ── Internal weapon state (truth) ────────────────────────────────── */

struct ax_weapon_internal {
    uint32_t player_id;
    uint32_t weapon_slot;

    int32_t  ammo_in_mag;
    int32_t  ammo_reserve;

    bool     reloading;
    uint32_t reload_ticks_remaining;
};

/* ── The real ax_core struct ──────────────────────────────────────── */

struct ax_core {
    ax_lifecycle lifecycle;

    /* logging */
    ax_log_fn log_fn;
    void*     log_user;

    /* simulation */
    uint64_t tick;

    /* entities (truth) */
    std::vector<ax_entity_internal> entities;

    /* player weapon (truth, A1: single weapon slot 0) */
    ax_weapon_internal weapon;

    /* pending actions for upcoming ticks */
    std::vector<ax_action_v1> action_queue;

    /* events emitted during the current tick */
    std::vector<ax_snapshot_event_v1> events;
};

/* ── Last error ───────────────────────────────────────────────────── */

const char* ax_get_last_error(void) {
    return g_last_error;
}

/* ── ABI version ──────────────────────────────────────────────────── */

ax_abi_version ax_get_abi_version(void) {
    ax_abi_version v;
    v.major = AX_ABI_MAJOR;
    v.minor = AX_ABI_MINOR;
    return v;
}

/* ── Core lifecycle ───────────────────────────────────────────────── */

ax_result ax_create(const ax_create_params_v1* params, ax_core** out_core) {
    /* null checks */
    if (!params || !out_core) {
        set_last_error("ax_create: params and out_core must not be NULL");
        return AX_ERR_INVALID_ARG;
    }

    /* struct version check */
    if (params->version != 1) {
        set_last_error("ax_create: unknown params version %u", params->version);
        return AX_ERR_UNSUPPORTED;
    }

    /* struct size check (forward compat: allow larger, reject smaller) */
    if (params->size_bytes < sizeof(ax_create_params_v1)) {
        set_last_error("ax_create: size_bytes %u < expected %u",
                       params->size_bytes,
                       (unsigned)sizeof(ax_create_params_v1));
        return AX_ERR_INVALID_ARG;
    }

    /* ABI major compatibility */
    if (params->abi_major != AX_ABI_MAJOR) {
        set_last_error("ax_create: ABI major mismatch (shell=%u, core=%u)",
                       params->abi_major, AX_ABI_MAJOR);
        return AX_ERR_UNSUPPORTED;
    }

    /* allocate and initialize */
    ax_core* core = new (std::nothrow) ax_core();
    if (!core) {
        set_last_error("ax_create: allocation failed");
        return AX_ERR_INTERNAL;
    }

    core->lifecycle = AX_LIFECYCLE_CREATED;
    core->log_fn    = params->log_fn;
    core->log_user  = params->log_user;
    core->tick      = 0;

    /* zero-initialize weapon state */
    std::memset(&core->weapon, 0, sizeof(core->weapon));

    *out_core = core;
    g_last_error[0] = '\0';    /* clear last error on success */
    return AX_OK;
}

void ax_destroy(ax_core* core) {
    if (!core) return;
    delete core;
}

/* ── Content loading ──────────────────────────────────────────────── */

ax_result ax_load_content(ax_core* core, const ax_content_load_params_v1* params) {
    if (!core || !params) {
        set_last_error("ax_load_content: core and params must not be NULL");
        return AX_ERR_INVALID_ARG;
    }

    /* lifecycle check: must be in CREATED state */
    if (core->lifecycle != AX_LIFECYCLE_CREATED) {
        set_last_error("ax_load_content: content already loaded (unload first)");
        return AX_ERR_BAD_STATE;
    }

    /* struct version check */
    if (params->version != 1) {
        set_last_error("ax_load_content: unknown params version %u", params->version);
        return AX_ERR_UNSUPPORTED;
    }

    /* struct size check */
    if (params->size_bytes < sizeof(ax_content_load_params_v1)) {
        set_last_error("ax_load_content: size_bytes %u < expected %u",
                       params->size_bytes,
                       (unsigned)sizeof(ax_content_load_params_v1));
        return AX_ERR_INVALID_ARG;
    }

    /* root_path check */
    if (!params->root_path || params->root_path[0] == '\0') {
        set_last_error("ax_load_content: root_path must not be NULL or empty");
        return AX_ERR_INVALID_ARG;
    }

    /*
     * TODO: Real content loading (A1 implementation)
     *   - read manifest.json from root_path
     *   - parse weapon records from weapons/<id>.json
     *   - parse target records from targets/<id>.json
     *   - validate all required fields per CONTENT_DATABASE.md
     *   - fail fast on missing/malformed records
     *
     * For now: spawn hardcoded placeholder entities so the
     * snapshot pipeline and headless shell have data to work with.
     */

    /* clear any stale state */
    core->entities.clear();
    core->action_queue.clear();
    core->events.clear();
    core->tick = 0;

    /* placeholder player entity (id=1) */
    ax_entity_internal player = {};
    player.id          = 1;
    player.archetype_id = 0;       /* no content record for player in A1 */
    player.px = 0.0f;  player.py = 0.0f;  player.pz = 0.0f;
    player.rx = 0.0f;  player.ry = 0.0f;  player.rz = 0.0f;  player.rw = 1.0f;
    player.hp          = -1;       /* not applicable for player */
    player.state_flags = AX_ENT_FLAG_PLAYER;
    core->entities.push_back(player);

    /* placeholder target entities (ids=100,101,102) */
    const uint32_t TARGET_ARCHETYPE = 2000;  /* matches CONTENT_DATABASE example */
    const int32_t  TARGET_HP        = 50;    /* matches target record max_hp     */
    const float    target_positions[][3] = {
        { 0.0f, 0.0f, -10.0f },
        { 5.0f, 0.0f, -15.0f },
        {-5.0f, 0.0f, -20.0f }
    };

    for (uint32_t i = 0; i < 3; ++i) {
        ax_entity_internal target = {};
        target.id           = 100 + i;
        target.archetype_id = TARGET_ARCHETYPE;
        target.px = target_positions[i][0];
        target.py = target_positions[i][1];
        target.pz = target_positions[i][2];
        target.rx = 0.0f;  target.ry = 0.0f;  target.rz = 0.0f;  target.rw = 1.0f;
        target.hp           = TARGET_HP;
        target.state_flags  = AX_ENT_FLAG_TARGET;
        core->entities.push_back(target);
    }

    /* placeholder weapon state (matches CONTENT_DATABASE weapon 1000) */
    core->weapon.player_id              = 1;
    core->weapon.weapon_slot            = 0;
    core->weapon.ammo_in_mag            = 12;    /* magazine_size    */
    core->weapon.ammo_reserve           = 48;    /* 4 extra mags     */
    core->weapon.reloading              = false;
    core->weapon.reload_ticks_remaining = 0;

    core->lifecycle = AX_LIFECYCLE_CONTENT_LOADED;
    g_last_error[0] = '\0';
    return AX_OK;
}

ax_result ax_unload_content(ax_core* core) {
    if (!core) {
        set_last_error("ax_unload_content: core must not be NULL");
        return AX_ERR_INVALID_ARG;
    }

    /* idempotent: unloading when nothing is loaded is fine */
    core->entities.clear();
    core->action_queue.clear();
    core->events.clear();
    core->tick = 0;
    std::memset(&core->weapon, 0, sizeof(core->weapon));

    core->lifecycle = AX_LIFECYCLE_CREATED;
    g_last_error[0] = '\0';
    return AX_OK;
}

/* ── Helpers ───────────────────────────────────────────────────────── */

static bool is_finite(float f) {
    return std::isfinite(f);
}

/* ── Action submission ────────────────────────────────────────────── */

ax_result ax_submit_actions(ax_core* core, const ax_action_batch_v1* batch) {
    if (!core || !batch) {
        set_last_error("ax_submit_actions: core and batch must not be NULL");
        return AX_ERR_INVALID_ARG;
    }

    /* lifecycle check: must have content loaded */
    if (core->lifecycle < AX_LIFECYCLE_CONTENT_LOADED) {
        set_last_error("ax_submit_actions: content not loaded");
        return AX_ERR_BAD_STATE;
    }

    /* struct version check */
    if (batch->version != 1) {
        set_last_error("ax_submit_actions: unknown batch version %u", batch->version);
        return AX_ERR_UNSUPPORTED;
    }

    /* struct size check */
    if (batch->size_bytes < sizeof(ax_action_batch_v1)) {
        set_last_error("ax_submit_actions: size_bytes %u < expected %u",
                       batch->size_bytes,
                       (unsigned)sizeof(ax_action_batch_v1));
        return AX_ERR_INVALID_ARG;
    }

    /* empty batch is fine — nothing to do */
    if (batch->count == 0) {
        return AX_OK;
    }

    /* actions pointer check */
    if (!batch->actions) {
        set_last_error("ax_submit_actions: count=%u but actions is NULL",
                       batch->count);
        return AX_ERR_INVALID_ARG;
    }

    /* per-action structural validation */
    for (uint32_t i = 0; i < batch->count; ++i) {
        const ax_action_v1* a = &batch->actions[i];

        /* type must be known */
        if (a->type < AX_ACT_MOVE_INTENT || a->type > AX_ACT_CROUCH_TOGGLE) {
            set_last_error("ax_submit_actions: action[%u] unknown type %u", i, a->type);
            return AX_ERR_INVALID_ARG;
        }

        /* float fields must be finite */
        if (a->type == AX_ACT_MOVE_INTENT) {
            if (!is_finite(a->u.move.x) || !is_finite(a->u.move.y)) {
                set_last_error("ax_submit_actions: action[%u] MOVE has non-finite values", i);
                return AX_ERR_INVALID_ARG;
            }
        }
        if (a->type == AX_ACT_LOOK_INTENT) {
            if (!is_finite(a->u.look.yaw) || !is_finite(a->u.look.pitch)) {
                set_last_error("ax_submit_actions: action[%u] LOOK has non-finite values", i);
                return AX_ERR_INVALID_ARG;
            }
        }

        /* queue the action */
        core->action_queue.push_back(*a);
    }

    return AX_OK;
}

/* ── Simulation stepping ──────────────────────────────────────────── */

ax_result ax_step_ticks(ax_core* core, uint32_t n_ticks) {
    if (!core) {
        set_last_error("ax_step_ticks: core must not be NULL");
        return AX_ERR_INVALID_ARG;
    }

    /* lifecycle check: must have content loaded */
    if (core->lifecycle < AX_LIFECYCLE_CONTENT_LOADED) {
        set_last_error("ax_step_ticks: content not loaded");
        return AX_ERR_BAD_STATE;
    }

    /* stepping 0 ticks is a no-op */
    if (n_ticks == 0) {
        return AX_OK;
    }

    for (uint32_t step = 0; step < n_ticks; ++step) {
        core->tick++;
        core->events.clear();

        /*
         * Process actions for this tick, in submission order.
         * COMBAT_A1 tick ordering:
         *   1) process actions in batch order
         *   2) advance timers (reload countdown)
         */
        for (size_t i = 0; i < core->action_queue.size(); ) {
            const ax_action_v1& a = core->action_queue[i];

            if (a.tick != core->tick) {
                ++i;
                continue;
            }

            /* ── MOVE_INTENT ── */
            if (a.type == AX_ACT_MOVE_INTENT) {
                for (auto& e : core->entities) {
                    if (e.id == a.actor_id && (e.state_flags & AX_ENT_FLAG_PLAYER)) {
                        /*
                         * TODO: Full A1 movement (COMBAT_A1 Movement Rules)
                         *   - normalize/clamp magnitude to 1.0
                         *   - rotate input by player yaw
                         *   - multiply by walk_speed_m_per_tick
                         *   - clamp y=0 (flat ground plane)
                         *
                         * Stub: apply input directly to XZ at fixed speed.
                         */
                        const float WALK_SPEED = 0.1f;  /* placeholder m/tick */
                        float mx = a.u.move.x;
                        float my = a.u.move.y;

                        /* clamp magnitude to 1.0 */
                        float mag = std::sqrt(mx * mx + my * my);
                        if (mag > 1.0f) {
                            mx /= mag;
                            my /= mag;
                        }

                        e.px += mx * WALK_SPEED;
                        e.pz += my * WALK_SPEED;
                        e.py = 0.0f;  /* clamp to ground (COMBAT_A1) */
                        break;
                    }
                }
            }

            /* ── LOOK_INTENT ── */
            else if (a.type == AX_ACT_LOOK_INTENT) {
                /*
                 * TODO: Full A1 look (COMBAT_A1 Controls)
                 *   - apply delta yaw/pitch to truth orientation
                 *   - update quaternion properly
                 *   - clamp pitch to prevent gimbal lock
                 *
                 * Stub: store yaw in quaternion Y component (placeholder).
                 * This is NOT a correct quaternion — just enough for
                 * the snapshot to show a non-zero rotation.
                 */
                for (auto& e : core->entities) {
                    if (e.id == a.actor_id && (e.state_flags & AX_ENT_FLAG_PLAYER)) {
                        e.ry += a.u.look.yaw;
                        break;
                    }
                }
            }

            /* ── FIRE_ONCE ── */
            else if (a.type == AX_ACT_FIRE_ONCE) {
                /* check blocked conditions (COMBAT_A1 Fire Rules) */
                if (core->weapon.reloading) {
                    ax_snapshot_event_v1 evt = {};
                    evt.type  = AX_EVT_FIRE_BLOCKED;
                    evt.a     = a.actor_id;
                    evt.b     = a.u.fire_once.weapon_slot;
                    evt.value = AX_FIRE_BLOCKED_RELOADING;
                    core->events.push_back(evt);
                }
                else if (core->weapon.ammo_in_mag <= 0) {
                    ax_snapshot_event_v1 evt = {};
                    evt.type  = AX_EVT_FIRE_BLOCKED;
                    evt.a     = a.actor_id;
                    evt.b     = a.u.fire_once.weapon_slot;
                    evt.value = AX_FIRE_BLOCKED_EMPTY_MAG;
                    core->events.push_back(evt);
                }
                else {
                    core->weapon.ammo_in_mag--;

                    /*
                     * TODO: Full A1 hitscan (COMBAT_A1 Hitscan Rules)
                     *   - compute ray from player truth pose + eye offset
                     *   - ray-sphere intersection against each living target
                     *   - select closest hit within max_range_m
                     *
                     * Stub: hit the first living target (if any).
                     */
                    const int32_t DAMAGE = 10;  /* placeholder damage_per_hit */

                    for (auto& e : core->entities) {
                        if ((e.state_flags & AX_ENT_FLAG_TARGET) &&
                            !(e.state_flags & AX_ENT_FLAG_DEAD)) {

                            e.hp -= DAMAGE;

                            ax_snapshot_event_v1 dmg = {};
                            dmg.type  = AX_EVT_DAMAGE_DEALT;
                            dmg.a     = a.actor_id;
                            dmg.b     = e.id;
                            dmg.value = DAMAGE;
                            core->events.push_back(dmg);

                            if (e.hp <= 0) {
                                e.state_flags |= AX_ENT_FLAG_DEAD;

                                ax_snapshot_event_v1 dest = {};
                                dest.type  = AX_EVT_TARGET_DESTROY;
                                dest.a     = a.actor_id;
                                dest.b     = e.id;
                                dest.value = 0;
                                core->events.push_back(dest);
                            }
                            break;  /* one hit per shot */
                        }
                    }
                }
            }

            /* ── RELOAD ── */
            else if (a.type == AX_ACT_RELOAD) {
                /* COMBAT_A1 Reload Rules */
                if (!core->weapon.reloading &&
                    core->weapon.ammo_in_mag < 12 &&  /* TODO: use content magazine_size */
                    core->weapon.ammo_reserve > 0) {

                    core->weapon.reloading              = true;
                    core->weapon.reload_ticks_remaining  = 30;  /* TODO: use content reload_duration_ticks */

                    ax_snapshot_event_v1 evt = {};
                    evt.type  = AX_EVT_RELOAD_STARTED;
                    evt.a     = a.actor_id;
                    evt.b     = a.u.reload.weapon_slot;
                    evt.value = 0;
                    core->events.push_back(evt);
                }
            }

            /* ── SPRINT / CROUCH (optional, no-op for now) ── */
            /* else if (a.type == AX_ACT_SPRINT_HELD) { } */
            /* else if (a.type == AX_ACT_CROUCH_TOGGLE) { } */

            /* remove processed action from queue */
            core->action_queue.erase(core->action_queue.begin() + i);
            /* don't increment i — next element shifted into this slot */
        }

        /*
         * Advance timers (COMBAT_A1 tick ordering step 2).
         * This runs AFTER all actions are processed.
         * Implication: RELOAD then FIRE_ONCE in same tick →
         *   FIRE sees reloading==true and is blocked.
         */
        if (core->weapon.reloading) {
            if (core->weapon.reload_ticks_remaining > 0) {
                core->weapon.reload_ticks_remaining--;
            }
            if (core->weapon.reload_ticks_remaining == 0) {
                /* COMBAT_A1 reload completion */
                int32_t magazine_size = 12;  /* TODO: use content value */
                int32_t needed  = magazine_size - core->weapon.ammo_in_mag;
                int32_t to_load = needed < core->weapon.ammo_reserve
                                    ? needed : core->weapon.ammo_reserve;

                core->weapon.ammo_in_mag  += to_load;
                core->weapon.ammo_reserve -= to_load;
                core->weapon.reloading     = false;

                ax_snapshot_event_v1 evt = {};
                evt.type  = AX_EVT_RELOAD_DONE;
                evt.a     = core->weapon.player_id;
                evt.b     = core->weapon.weapon_slot;
                evt.value = to_load;
                core->events.push_back(evt);
            }
        }
    }

    /* transition to RUNNING after first tick */
    if (core->lifecycle == AX_LIFECYCLE_CONTENT_LOADED) {
        core->lifecycle = AX_LIFECYCLE_RUNNING;
    }

    g_last_error[0] = '\0';
    return AX_OK;
}

/* ── Snapshots ────────────────────────────────────────────────────── */

ax_result ax_get_snapshot_bytes(
    ax_core*  core,
    void*     out_buf,
    uint32_t  out_cap_bytes,
    uint32_t* out_size_bytes)
{
    if (!core) {
        set_last_error("ax_get_snapshot_bytes: core must not be NULL");
        return AX_ERR_INVALID_ARG;
    }
    if (!out_size_bytes) {
        set_last_error("ax_get_snapshot_bytes: out_size_bytes must not be NULL");
        return AX_ERR_INVALID_ARG;
    }

    /* lifecycle check: must have content loaded */
    if (core->lifecycle < AX_LIFECYCLE_CONTENT_LOADED) {
        set_last_error("ax_get_snapshot_bytes: content not loaded");
        return AX_ERR_BAD_STATE;
    }

    /* determine if player weapon state is present */
    uint32_t has_weapon = 0;
    for (const auto& e : core->entities) {
        if (e.state_flags & AX_ENT_FLAG_PLAYER) {
            has_weapon = 1;
            break;
        }
    }

    /* compute total blob size */
    uint32_t entity_count = (uint32_t)core->entities.size();
    uint32_t event_count  = (uint32_t)core->events.size();

    uint32_t total = (uint32_t)sizeof(ax_snapshot_header_v1)
                   + entity_count * (uint32_t)sizeof(ax_snapshot_entity_v1)
                   + has_weapon   * (uint32_t)sizeof(ax_snapshot_player_weapon_v1)
                   + event_count  * (uint32_t)sizeof(ax_snapshot_event_v1);

    /* always write required size (buffer-too-small rule) */
    *out_size_bytes = total;

    /* size-query path: out_buf is NULL */
    if (!out_buf) {
        return AX_OK;
    }

    /* buffer too small */
    if (out_cap_bytes < total) {
        set_last_error("ax_get_snapshot_bytes: buffer too small (%u < %u)",
                       out_cap_bytes, total);
        return AX_ERR_BUFFER_TOO_SMALL;
    }

    /* ── Build the blob ───────────────────────────────────────────── */

    uint8_t* dst = (uint8_t*)out_buf;
    uint32_t offset = 0;

    /* header */
    ax_snapshot_header_v1 hdr = {};
    hdr.version              = 1;
    hdr.reserved             = 0;
    hdr.size_bytes           = total;
    hdr.tick                 = core->tick;
    hdr.entity_count         = entity_count;
    hdr.entity_stride_bytes  = (uint32_t)sizeof(ax_snapshot_entity_v1);
    hdr.event_count          = event_count;
    hdr.event_stride_bytes   = (uint32_t)sizeof(ax_snapshot_event_v1);
    hdr.flags                = 0;
    hdr.player_weapon_present = has_weapon;

    std::memcpy(dst + offset, &hdr, sizeof(hdr));
    offset += (uint32_t)sizeof(hdr);

    /* entities */
    for (uint32_t i = 0; i < entity_count; ++i) {
        const ax_entity_internal& src = core->entities[i];

        ax_snapshot_entity_v1 ent = {};
        ent.id           = src.id;
        ent.archetype_id = src.archetype_id;
        ent.px = src.px;  ent.py = src.py;  ent.pz = src.pz;
        ent.rx = src.rx;  ent.ry = src.ry;  ent.rz = src.rz;  ent.rw = src.rw;
        ent.hp           = src.hp;
        ent.state_flags  = src.state_flags;

        std::memcpy(dst + offset, &ent, sizeof(ent));
        offset += (uint32_t)sizeof(ent);
    }

    /* player weapon state (if present) */
    if (has_weapon) {
        ax_snapshot_player_weapon_v1 wpn = {};
        wpn.player_id    = core->weapon.player_id;
        wpn.weapon_slot  = core->weapon.weapon_slot;
        wpn.ammo_in_mag  = core->weapon.ammo_in_mag;
        wpn.ammo_reserve = core->weapon.ammo_reserve;

        wpn.weapon_flags = 0;
        if (core->weapon.reloading) {
            wpn.weapon_flags |= AX_WPN_FLAG_RELOADING;
        }

        /*
         * reload_progress: 0.0 .. 1.0 for presentation.
         * Internally tracked as integer ticks remaining (D111).
         * Convert for the snapshot.
         */
        if (core->weapon.reloading && core->weapon.reload_ticks_remaining > 0) {
            uint32_t total_ticks = 30;  /* TODO: use content reload_duration_ticks */
            wpn.reload_progress = 1.0f - ((float)core->weapon.reload_ticks_remaining
                                          / (float)total_ticks);
        } else {
            wpn.reload_progress = 0.0f;
        }

        std::memcpy(dst + offset, &wpn, sizeof(wpn));
        offset += (uint32_t)sizeof(wpn);
    }

    /* events */
    for (uint32_t i = 0; i < event_count; ++i) {
        std::memcpy(dst + offset, &core->events[i], sizeof(ax_snapshot_event_v1));
        offset += (uint32_t)sizeof(ax_snapshot_event_v1);
    }

    g_last_error[0] = '\0';
    return AX_OK;
}

/* ── Save / Load (stubs until SAVE_FORMAT.md implementation) ──────── */

ax_result ax_save_bytes(
    ax_core*  core,
    void*     out_buf,
    uint32_t  out_cap_bytes,
    uint32_t* out_size_bytes)
{
    if (!core) {
        set_last_error("ax_save_bytes: core must not be NULL");
        return AX_ERR_INVALID_ARG;
    }
    if (!out_size_bytes) {
        set_last_error("ax_save_bytes: out_size_bytes must not be NULL");
        return AX_ERR_INVALID_ARG;
    }

    /* lifecycle check: must have content loaded */
    if (core->lifecycle < AX_LIFECYCLE_CONTENT_LOADED) {
        set_last_error("ax_save_bytes: content not loaded");
        return AX_ERR_BAD_STATE;
    }

    /*
     * TODO: Implement SAVE_FORMAT.md v0.3
     *   - write SaveHeaderV1 (magic, version, checksum)
     *   - write A1WorldV1 (tick, player transform, weapon state)
     *   - write TargetsV1[] (entity_id, transform, hp, flags)
     *   - support size-query (out_buf == NULL)
     *   - support buffer-too-small rule
     */
    (void)out_buf;
    (void)out_cap_bytes;
    *out_size_bytes = 0;
    set_last_error("ax_save_bytes: not yet implemented");
    return AX_ERR_UNSUPPORTED;
}

ax_result ax_load_save_bytes(
    ax_core*    core,
    const void* save_buf,
    uint32_t    save_size_bytes)
{
    if (!core) {
        set_last_error("ax_load_save_bytes: core must not be NULL");
        return AX_ERR_INVALID_ARG;
    }
    if (!save_buf) {
        set_last_error("ax_load_save_bytes: save_buf must not be NULL");
        return AX_ERR_INVALID_ARG;
    }
    if (save_size_bytes == 0) {
        set_last_error("ax_load_save_bytes: save_size_bytes must be > 0");
        return AX_ERR_INVALID_ARG;
    }

    /* lifecycle check: content must be loaded first (SAVE_FORMAT dependency rule) */
    if (core->lifecycle < AX_LIFECYCLE_CONTENT_LOADED) {
        set_last_error("ax_load_save_bytes: content must be loaded before loading a save");
        return AX_ERR_BAD_STATE;
    }

    /*
     * TODO: Implement SAVE_FORMAT.md v0.3
     *   - validate SaveHeaderV1 (magic 'AXSV', version, checksum)
     *   - read A1WorldV1 (restore tick, player transform, weapon state)
     *   - read TargetsV1[] (restore entity states)
     *   - verify content references exist
     *   - fail non-destructively (preserve current state on error)
     */
    set_last_error("ax_load_save_bytes: not yet implemented");
    return AX_ERR_UNSUPPORTED;
}

/* ── Diagnostics ──────────────────────────────────────────────────── */

ax_result ax_get_diagnostics(ax_core* core, ax_diagnostics_v1* out_diag) {
    if (!core) {
        set_last_error("ax_get_diagnostics: core must not be NULL");
        return AX_ERR_INVALID_ARG;
    }
    if (!out_diag) {
        set_last_error("ax_get_diagnostics: out_diag must not be NULL");
        return AX_ERR_INVALID_ARG;
    }

    std::memset(out_diag, 0, sizeof(ax_diagnostics_v1));

    out_diag->version       = 1;
    out_diag->reserved      = 0;
    out_diag->size_bytes    = (uint32_t)sizeof(ax_diagnostics_v1);

    out_diag->abi_major     = AX_ABI_MAJOR;
    out_diag->abi_minor     = AX_ABI_MINOR;

    out_diag->current_tick  = core->tick;

    out_diag->feature_flags = 0;  /* none defined in v1 */

    /*
     * Build hash: injected at compile time via -DAX_BUILD_HASH="..."
     * Falls back to "unknown" if not defined.
     */
#ifdef AX_BUILD_HASH
    snprintf(out_diag->build_hash, AX_BUILD_HASH_LEN, "%s", AX_BUILD_HASH);
#else
    snprintf(out_diag->build_hash, AX_BUILD_HASH_LEN, "unknown");
#endif

    snprintf(out_diag->version_string, AX_VERSION_STRING_LEN, "Axiom Core 0.1.0-dev");

    g_last_error[0] = '\0';
    return AX_OK;
}