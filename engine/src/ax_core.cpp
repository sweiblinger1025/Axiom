#include "ax_abi.h"
#include "ax_last_error.h"

#include <vector>
#include <cstring>
#include <algorithm>

struct ax_core {
    ax_last_error last;

    ax_log_fn log_fn = nullptr;
    void* log_user = nullptr;

    uint64_t tick = 0;

    // Stored next-tick actions (copy of bytes)
    std::vector<uint8_t> next_actions_blob;
    uint32_t next_action_count = 0;
    uint32_t next_action_stride = 0;

    // Minimal A1 world truth (stub values)
    struct ent {
        uint32_t id;
        uint32_t kind; // 0 player, 1 target
        float px,py,pz;
        float rx,ry,rz,rw;
        int32_t hp;
        uint32_t flags;
    };
    std::vector<ent> ents;

    // weapon truth
    uint32_t weapon_id = 0;
    int32_t ammo_in_mag = 0;
    int32_t ammo_reserve = 0;
    uint32_t reload_ticks_remaining = 0;

    // events produced on last tick
    std::vector<ax_snapshot_event_v1> events;

    bool content_loaded = false;
    std::string content_root;
};

static void ax_log(ax_core* c, int level, const char* msg) {
    if (c && c->log_fn) c->log_fn(c->log_user, level, msg);
}

ax_abi_version ax_get_abi_version(void) {
    ax_abi_version v;
    v.major = AX_ABI_MAJOR;
    v.minor = AX_ABI_MINOR;
    return v;
}

ax_result ax_create(const ax_create_params_v1* params, ax_core** out_core) {
    if (!out_core) return AX_ERR_INVALID_ARGUMENT;
    *out_core = nullptr;

    if (!params || params->version != 1 || params->size_bytes < sizeof(ax_create_params_v1)) {
        return AX_ERR_INVALID_ARGUMENT;
    }

    auto* c = new (std::nothrow) ax_core();
    if (!c) return AX_ERR_INTERNAL;

    c->log_fn = params->log_fn;
    c->log_user = params->log_user;

    // Seed minimal A1 entities: player + one target (placeholder)
    ax_core::ent player{1,0, 0,0,0, 0,0,0,1, 100, 0};
    ax_core::ent target{2,1, 5,0,0, 0,0,0,1, 50, 0};
    c->ents.push_back(player);
    c->ents.push_back(target);

    // Placeholder weapon truth
    c->weapon_id = 1000;
    c->ammo_in_mag = 12;
    c->ammo_reserve = 120;
    c->reload_ticks_remaining = 0;

    *out_core = c;
    return AX_OK;
}

void ax_destroy(ax_core* core) {
    delete core;
}

const char* ax_get_last_error(ax_core* core) {
    if (!core) return "ax_core is null";
    return core->last.c_str();
}

ax_result ax_submit_actions_next_tick(ax_core* core, const ax_action_batch_v1* batch) {
    if (!core || !batch) return AX_ERR_INVALID_ARGUMENT;
    if (batch->version != 1 || batch->size_bytes < sizeof(ax_action_batch_v1)) {
        core->last.set("Invalid action batch header (version/size).");
        return AX_ERR_INVALID_ARGUMENT;
    }
    if (batch->action_count > 0 && (!batch->actions || batch->action_stride_bytes == 0)) {
        core->last.set("Invalid action batch payload (null pointer or stride 0).");
        return AX_ERR_INVALID_ARGUMENT;
    }
    if (batch->action_stride_bytes != sizeof(ax_action_v1)) {
        core->last.set("action_stride_bytes must equal sizeof(ax_action_v1) in v1.");
        return AX_ERR_INVALID_ARGUMENT;
    }

    const size_t bytes = (size_t)batch->action_count * (size_t)batch->action_stride_bytes;
    core->next_actions_blob.resize(bytes);
    core->next_action_count = batch->action_count;
    core->next_action_stride = batch->action_stride_bytes;

    if (bytes > 0) {
        std::memcpy(core->next_actions_blob.data(), batch->actions, bytes);
    }

    return AX_OK;
}

static void clear_events(ax_core* c) {
    c->events.clear();
}

static void push_event(ax_core* c, uint16_t type, uint32_t a, int32_t b) {
    ax_snapshot_event_v1 e{};
    e.type = type;
    e.a = a;
    e.b = b;
    c->events.push_back(e);
}

static ax_core::ent* find_player(ax_core* c) {
    for (auto& e : c->ents) if (e.kind == 0) return &e;
    return nullptr;
}

ax_result ax_step_ticks(ax_core* core, uint32_t n_ticks) {
    if (!core) return AX_ERR_INVALID_ARGUMENT;
    if (n_ticks == 0) return AX_OK;

    for (uint32_t i = 0; i < n_ticks; ++i) {
        // Advance tick boundary
        core->tick += 1;
        clear_events(core);

        // Apply next-tick actions (A1 scheduling)
        // NOTE: This is stub behavior: we don't implement movement/combat yet.
        // We DO demonstrate action order + basic validation plumbing.
        const uint8_t* p = core->next_actions_blob.data();
        const uint32_t stride = core->next_action_stride;
        for (uint32_t j = 0; j < core->next_action_count; ++j) {
            const ax_action_v1* act = reinterpret_cast<const ax_action_v1*>(p + (size_t)j * stride);
            switch (act->type) {
                case AX_ACT_RELOAD:
                    if (core->reload_ticks_remaining == 0) {
                        core->reload_ticks_remaining = 30; // TODO: from weapon content
                        push_event(core, AX_EVT_RELOAD_STARTED, act->u.reload.weapon_slot, 0);
                    }
                    break;
                case AX_ACT_FIRE_ONCE:
                    if (core->reload_ticks_remaining > 0 || core->ammo_in_mag <= 0) {
                        // Fire blocked
                        push_event(core, AX_EVT_FIRE_BLOCKED, act->u.fire_once.weapon_slot,
                                   (core->reload_ticks_remaining > 0) ? 1 : 2);
                    } else {
                        // Placeholder: consume ammo and emit damage to target id=2
                        core->ammo_in_mag -= 1;
                        push_event(core, AX_EVT_DAMAGE, 2, 10);
                    }
                    break;
                default:
                    break;
            }
        }

        // Clear stored next tick actions after applying
        core->next_actions_blob.clear();
        core->next_action_count = 0;
        core->next_action_stride = 0;

        // Timers advance AFTER actions (COMBAT_A1)
        if (core->reload_ticks_remaining > 0) {
            core->reload_ticks_remaining -= 1;
            if (core->reload_ticks_remaining == 0) {
                // TODO: refill mag from reserve based on weapon content
                push_event(core, AX_EVT_RELOAD_COMPLETE, 0, 0);
            }
        }
    }

    return AX_OK;
}

ax_result ax_get_snapshot_bytes(ax_core* core, void* out_buf, uint32_t out_cap_bytes, uint32_t* out_size_bytes) {
    if (!core || !out_size_bytes) return AX_ERR_INVALID_ARGUMENT;

    const uint32_t header_sz = (uint32_t)sizeof(ax_snapshot_header_v1);
    const uint32_t ent_stride = (uint32_t)sizeof(ax_snapshot_entity_v1);
    const uint32_t evt_stride = (uint32_t)sizeof(ax_snapshot_event_v1);
    const uint32_t wpn_stride = (uint32_t)sizeof(ax_snapshot_player_weapon_v1);

    const uint32_t ent_bytes = (uint32_t)core->ents.size() * ent_stride;
    const uint32_t wpn_bytes = wpn_stride; // always present in A1 snapshot
    const uint32_t evt_bytes = (uint32_t)core->events.size() * evt_stride;

    const uint32_t total = header_sz + ent_bytes + wpn_bytes + evt_bytes;
    *out_size_bytes = total;

    if (!out_buf) return AX_OK;
    if (out_cap_bytes < total) return AX_ERR_BUFFER_TOO_SMALL;

    uint8_t* dst = (uint8_t*)out_buf;

    ax_snapshot_header_v1 h{};
    h.magic = 0x4E534841u; // 'AXSN'
    h.version = 1;
    h.size_bytes = total;
    h.tick = core->tick;
    h.entity_count = (uint32_t)core->ents.size();
    h.entity_stride_bytes = ent_stride;
    h.event_count = (uint32_t)core->events.size();
    h.event_stride_bytes = evt_stride;
    h.weapon_present = 1;
    h.weapon_stride_bytes = wpn_stride;

    std::memcpy(dst, &h, sizeof(h));
    size_t off = sizeof(h);

    // Entities
    for (const auto& e : core->ents) {
        ax_snapshot_entity_v1 se{};
        se.id = e.id;
        se.kind = e.kind;
        se.px = e.px; se.py = e.py; se.pz = e.pz;
        se.rx = e.rx; se.ry = e.ry; se.rz = e.rz; se.rw = e.rw;
        se.hp = e.hp;
        se.flags = e.flags;
        std::memcpy(dst + off, &se, sizeof(se));
        off += sizeof(se);
    }

    // Player weapon
    ax_snapshot_player_weapon_v1 w{};
    w.weapon_id = core->weapon_id;
    w.ammo_in_mag = core->ammo_in_mag;
    w.ammo_reserve = core->ammo_reserve;
    w.reload_ticks_remaining = core->reload_ticks_remaining;
    w.reload_progress = (w.reload_ticks_remaining > 0) ? 0.0f : 1.0f; // TODO: derived
    std::memcpy(dst + off, &w, sizeof(w));
    off += sizeof(w);

    // Events
    if (!core->events.empty()) {
        std::memcpy(dst + off, core->events.data(), core->events.size() * sizeof(ax_snapshot_event_v1));
        off += core->events.size() * sizeof(ax_snapshot_event_v1);
    }

    return AX_OK;
}

ax_result ax_load_content(ax_core* core, const ax_content_load_params_v1* params) {
    if (!core || !params) return AX_ERR_INVALID_ARGUMENT;
    if (params->version != 1 || params->size_bytes < sizeof(ax_content_load_params_v1) || !params->root_path) {
        core->last.set("Invalid content load params.");
        return AX_ERR_INVALID_ARGUMENT;
    }
    // TODO: parse manifest.json and load weapon/target defs per CONTENT_DATABASE.md
    core->content_loaded = true;
    core->content_root = params->root_path;
    ax_log(core, 1, "ax_load_content: stub success (no parsing yet).");
    return AX_OK;
}

ax_result ax_unload_content(ax_core* core) {
    if (!core) return AX_ERR_INVALID_ARGUMENT;
    core->content_loaded = false;
    core->content_root.clear();
    return AX_OK;
}

ax_result ax_save_bytes(ax_core* core, void* out_buf, uint32_t out_cap_bytes, uint32_t* out_size_bytes) {
    if (!core || !out_size_bytes) return AX_ERR_INVALID_ARGUMENT;
    // TODO: implement SAVE_FORMAT.md v0.3 (LOCKED)
    *out_size_bytes = 0;
    core->last.set("ax_save_bytes not implemented yet.");
    return AX_ERR_UNSUPPORTED;
}

ax_result ax_load_save_bytes(ax_core* core, const void* save_buf, uint32_t save_size_bytes) {
    if (!core || !save_buf || save_size_bytes == 0) return AX_ERR_INVALID_ARGUMENT;
    // TODO: implement SAVE_FORMAT.md v0.3 (LOCKED)
    core->last.set("ax_load_save_bytes not implemented yet.");
    return AX_ERR_UNSUPPORTED;
}
