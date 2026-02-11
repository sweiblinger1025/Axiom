/*
 * main.cpp — Axiom Headless Shell (A1 test harness)
 *
 * This is a first-class app shell (ARCHITECTURE.md) that exercises
 * the full Core lifecycle without any rendering. Used for:
 *   - determinism tests
 *   - replay validation
 *   - CI acceptance checks
 *
 * Authoritative spec: COMBAT_A1.md v0.4 (acceptance criteria)
 */

#include "ax_abi.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>

/* ── Result code to string ────────────────────────────────────────── */

static const char* result_str(ax_result r) {
    switch (r) {
        case AX_OK:                   return "AX_OK";
        case AX_ERR_INVALID_ARG:      return "AX_ERR_INVALID_ARG";
        case AX_ERR_BAD_STATE:        return "AX_ERR_BAD_STATE";
        case AX_ERR_UNSUPPORTED:      return "AX_ERR_UNSUPPORTED";
        case AX_ERR_BUFFER_TOO_SMALL: return "AX_ERR_BUFFER_TOO_SMALL";
        case AX_ERR_PARSE_FAILED:     return "AX_ERR_PARSE_FAILED";
        case AX_ERR_IO:               return "AX_ERR_IO";
        case AX_ERR_INTERNAL:         return "AX_ERR_INTERNAL";
        default:                      return "UNKNOWN";
    }
}

/* ── Test bookkeeping ─────────────────────────────────────────────── */

static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define CHECK(expr, fmt, ...)                                           \
    do {                                                                \
        g_tests_run++;                                                  \
        if (expr) {                                                     \
            g_tests_passed++;                                           \
        } else {                                                        \
            g_tests_failed++;                                           \
            printf("  FAIL [%s:%d]: " fmt "\n",                         \
                   __FILE__, __LINE__, ##__VA_ARGS__);                   \
        }                                                               \
    } while (0)

#define CHECK_OK(r)                                                     \
    CHECK((r) == AX_OK, "expected AX_OK, got %s", result_str(r))

#define CHECK_ERR(r, expected)                                          \
    CHECK((r) == (expected), "expected %s, got %s",                     \
          result_str(expected), result_str(r))

/* ── Snapshot parsing helpers ─────────────────────────────────────── */

struct parsed_snapshot {
    const ax_snapshot_header_v1*        header;
    const ax_snapshot_entity_v1*        entities;    /* array */
    const ax_snapshot_player_weapon_v1* weapon;      /* NULL if absent */
    const ax_snapshot_event_v1*         events;      /* array */
};

static parsed_snapshot parse_snapshot(const void* buf, uint32_t size) {
    parsed_snapshot snap = {};

    if (!buf || size < sizeof(ax_snapshot_header_v1)) {
        return snap;
    }

    const uint8_t* p = (const uint8_t*)buf;
    uint32_t offset = 0;

    /* header */
    snap.header = (const ax_snapshot_header_v1*)(p + offset);
    offset += sizeof(ax_snapshot_header_v1);

    /* entities */
    uint32_t entities_size = snap.header->entity_count * snap.header->entity_stride_bytes;
    if (offset + entities_size > size) return snap;
    snap.entities = (const ax_snapshot_entity_v1*)(p + offset);
    offset += entities_size;

    /* weapon (optional) */
    if (snap.header->player_weapon_present) {
        if (offset + sizeof(ax_snapshot_player_weapon_v1) > size) return snap;
        snap.weapon = (const ax_snapshot_player_weapon_v1*)(p + offset);
        offset += sizeof(ax_snapshot_player_weapon_v1);
    }

    /* events */
    uint32_t events_size = snap.header->event_count * snap.header->event_stride_bytes;
    if (offset + events_size > size) return snap;
    snap.events = (const ax_snapshot_event_v1*)(p + offset);

    return snap;
}

/* ── Lifecycle helpers ────────────────────────────────────────────── */

static ax_core* create_and_load(const char* content_path) {
    ax_create_params_v1 params = {};
    params.version    = 1;
    params.size_bytes = sizeof(params);
    params.abi_major  = AX_ABI_MAJOR;
    params.abi_minor  = AX_ABI_MINOR;
    params.log_fn     = nullptr;
    params.log_user   = nullptr;

    ax_core* core = nullptr;
    ax_result r = ax_create(&params, &core);
    if (r != AX_OK) {
        printf("  create_and_load: ax_create failed: %s\n", result_str(r));
        return nullptr;
    }

    ax_content_load_params_v1 content = {};
    content.version    = 1;
    content.size_bytes = sizeof(content);
    content.root_path  = content_path;

    r = ax_load_content(core, &content);
    if (r != AX_OK) {
        printf("  create_and_load: ax_load_content failed: %s\n", result_str(r));
        ax_destroy(core);
        return nullptr;
    }

    return core;
}

static std::vector<uint8_t> take_snapshot(ax_core* core) {
    uint32_t size = 0;
    ax_result r = ax_get_snapshot_bytes(core, nullptr, 0, &size);
    if (r != AX_OK || size == 0) {
        printf("  take_snapshot: size query failed: %s\n", result_str(r));
        return {};
    }

    std::vector<uint8_t> buf(size);
    r = ax_get_snapshot_bytes(core, buf.data(), size, &size);
    if (r != AX_OK) {
        printf("  take_snapshot: copy failed: %s\n", result_str(r));
        return {};
    }

    return buf;
}

static void submit_action(ax_core* core, const ax_action_v1& action) {
    ax_action_batch_v1 batch = {};
    batch.version    = 1;
    batch.size_bytes = sizeof(batch);
    batch.count      = 1;
    batch.actions    = &action;

    ax_result r = ax_submit_actions(core, &batch);
    if (r != AX_OK) {
        printf("  submit_action: failed: %s (%s)\n",
               result_str(r), ax_get_last_error());
    }
}

/* ══════════════════════════════════════════════════════════════════════
 * Test: basic fire + damage
 * COMBAT_A1 acceptance criteria #1
 * ══════════════════════════════════════════════════════════════════ */

static void test_basic_fire_and_damage(void) {
    printf("test_basic_fire_and_damage\n");

    ax_core* core = create_and_load("content/");
    CHECK(core != nullptr, "core creation failed");
    if (!core) return;

    /* verify initial state via snapshot */
    {
        auto buf = take_snapshot(core);
        CHECK(!buf.empty(), "initial snapshot failed");
        parsed_snapshot snap = parse_snapshot(buf.data(), (uint32_t)buf.size());
        CHECK(snap.header != nullptr, "snapshot parse failed");
        CHECK(snap.header->tick == 0, "initial tick should be 0, got %llu",
              (unsigned long long)snap.header->tick);
        CHECK(snap.header->entity_count == 4, "expected 4 entities, got %u",
              snap.header->entity_count);
        CHECK(snap.weapon != nullptr, "weapon block should be present");
        if (snap.weapon) {
            CHECK(snap.weapon->ammo_in_mag == 12, "initial ammo should be 12, got %d",
                  snap.weapon->ammo_in_mag);
            CHECK(snap.weapon->ammo_reserve == 48, "initial reserve should be 48, got %d",
                  snap.weapon->ammo_reserve);
        }
    }

    /* fire 5 shots, one per tick (ticks 1-5) */
    int32_t total_damage = 0;
    int32_t destroy_count = 0;

    for (uint32_t t = 1; t <= 5; ++t) {
        ax_action_v1 fire = {};
        fire.tick     = t;
        fire.actor_id = 1;       /* player */
        fire.type     = AX_ACT_FIRE_ONCE;
        fire.u.fire_once.weapon_slot = 0;

        submit_action(core, fire);
        ax_step_ticks(core, 1);

        /* check events for this tick */
        auto buf = take_snapshot(core);
        parsed_snapshot snap = parse_snapshot(buf.data(), (uint32_t)buf.size());

        for (uint32_t i = 0; i < snap.header->event_count; ++i) {
            const ax_snapshot_event_v1* evt = &snap.events[i];
            if (evt->type == AX_EVT_DAMAGE_DEALT) {
                total_damage += evt->value;
            }
            if (evt->type == AX_EVT_TARGET_DESTROY) {
                destroy_count++;
            }
        }
    }

    /* verify final state */
    CHECK(total_damage == 50, "total damage should be 50, got %d", total_damage);
    CHECK(destroy_count == 1, "should have destroyed 1 target, got %d", destroy_count);

    auto final_buf = take_snapshot(core);
    parsed_snapshot final_snap = parse_snapshot(final_buf.data(), (uint32_t)final_buf.size());

    CHECK(final_snap.header->tick == 5, "final tick should be 5, got %llu",
          (unsigned long long)final_snap.header->tick);

    /* check ammo */
    CHECK(final_snap.weapon != nullptr, "weapon block should be present");
    if (final_snap.weapon) {
        CHECK(final_snap.weapon->ammo_in_mag == 7, "ammo should be 7 (12-5), got %d",
              final_snap.weapon->ammo_in_mag);
        CHECK(final_snap.weapon->ammo_reserve == 48, "reserve should be unchanged at 48, got %d",
              final_snap.weapon->ammo_reserve);
    }

    /* check target HP and destroyed flag */
    for (uint32_t i = 0; i < final_snap.header->entity_count; ++i) {
        const ax_snapshot_entity_v1* ent = &final_snap.entities[i];

        if (ent->id == 100) {
            /* first target: should be destroyed */
            CHECK(ent->hp <= 0, "target 100 HP should be <= 0, got %d", ent->hp);
            CHECK((ent->state_flags & AX_ENT_FLAG_DEAD) != 0,
                  "target 100 should have DEAD flag set");
        }
        if (ent->id == 101) {
            /* second target: untouched */
            CHECK(ent->hp == 50, "target 101 HP should be 50, got %d", ent->hp);
            CHECK((ent->state_flags & AX_ENT_FLAG_DEAD) == 0,
                  "target 101 should NOT have DEAD flag");
        }
        if (ent->id == 102) {
            /* third target: untouched */
            CHECK(ent->hp == 50, "target 102 HP should be 50, got %d", ent->hp);
            CHECK((ent->state_flags & AX_ENT_FLAG_DEAD) == 0,
                  "target 102 should NOT have DEAD flag");
        }
    }

    ax_unload_content(core);
    ax_destroy(core);
    printf("  done\n");
}

/* ══════════════════════════════════════════════════════════════════════
 * Test: reload cycle
 * COMBAT_A1 fire rules + reload rules + tick ordering
 * ══════════════════════════════════════════════════════════════════ */

static void test_reload_cycle(void) {
    printf("test_reload_cycle\n");

    ax_core* core = create_and_load("content/");
    CHECK(core != nullptr, "core creation failed");
    if (!core) return;

    uint64_t tick = 0;

    /* ── Phase 1: fire 12 shots to empty the magazine ─────────────── */
    for (uint32_t i = 0; i < 12; ++i) {
        tick++;
        ax_action_v1 fire = {};
        fire.tick     = tick;
        fire.actor_id = 1;
        fire.type     = AX_ACT_FIRE_ONCE;
        fire.u.fire_once.weapon_slot = 0;

        submit_action(core, fire);
        ax_step_ticks(core, 1);
    }

    /* verify magazine is empty */
    {
        auto buf = take_snapshot(core);
        parsed_snapshot snap = parse_snapshot(buf.data(), (uint32_t)buf.size());
        CHECK(snap.weapon != nullptr, "weapon block missing");
        if (snap.weapon) {
            CHECK(snap.weapon->ammo_in_mag == 0, "mag should be 0 after 12 shots, got %d",
                  snap.weapon->ammo_in_mag);
            CHECK(snap.weapon->ammo_reserve == 48, "reserve should be 48, got %d",
                  snap.weapon->ammo_reserve);
        }
    }

    /* ── Phase 2: fire on empty → FIRE_BLOCKED (empty_mag) ────────── */
    tick++;
    {
        ax_action_v1 fire = {};
        fire.tick     = tick;
        fire.actor_id = 1;
        fire.type     = AX_ACT_FIRE_ONCE;
        fire.u.fire_once.weapon_slot = 0;

        submit_action(core, fire);
        ax_step_ticks(core, 1);

        auto buf = take_snapshot(core);
        parsed_snapshot snap = parse_snapshot(buf.data(), (uint32_t)buf.size());

        /* should have exactly one event: FIRE_BLOCKED with reason empty_mag */
        CHECK(snap.header->event_count == 1, "expected 1 event, got %u",
              snap.header->event_count);
        if (snap.header->event_count >= 1) {
            CHECK(snap.events[0].type == AX_EVT_FIRE_BLOCKED,
                  "expected FIRE_BLOCKED (5), got %u", snap.events[0].type);
            CHECK(snap.events[0].value == AX_FIRE_BLOCKED_EMPTY_MAG,
                  "expected reason empty_mag (2), got %d", snap.events[0].value);
        }
    }

    /* ── Phase 3: reload → RELOAD_STARTED ─────────────────────────── */
    tick++;
    {
        ax_action_v1 reload = {};
        reload.tick     = tick;
        reload.actor_id = 1;
        reload.type     = AX_ACT_RELOAD;
        reload.u.reload.weapon_slot = 0;

        submit_action(core, reload);
        ax_step_ticks(core, 1);

        auto buf = take_snapshot(core);
        parsed_snapshot snap = parse_snapshot(buf.data(), (uint32_t)buf.size());

        /* should have RELOAD_STARTED event */
        bool found_reload_started = false;
        for (uint32_t i = 0; i < snap.header->event_count; ++i) {
            if (snap.events[i].type == AX_EVT_RELOAD_STARTED) {
                found_reload_started = true;
                CHECK(snap.events[i].a == 1, "RELOAD_STARTED actor should be 1, got %u",
                      snap.events[i].a);
                CHECK(snap.events[i].b == 0, "RELOAD_STARTED weapon_slot should be 0, got %u",
                      snap.events[i].b);
            }
        }
        CHECK(found_reload_started, "RELOAD_STARTED event not found");

        /* weapon should show reloading flag */
        CHECK(snap.weapon != nullptr, "weapon block missing");
        if (snap.weapon) {
            CHECK((snap.weapon->weapon_flags & AX_WPN_FLAG_RELOADING) != 0,
                  "weapon should have RELOADING flag set");
        }
    }

    /* ── Phase 4: fire during reload → FIRE_BLOCKED (reloading) ───── */
    tick++;
    {
        ax_action_v1 fire = {};
        fire.tick     = tick;
        fire.actor_id = 1;
        fire.type     = AX_ACT_FIRE_ONCE;
        fire.u.fire_once.weapon_slot = 0;

        submit_action(core, fire);
        ax_step_ticks(core, 1);

        auto buf = take_snapshot(core);
        parsed_snapshot snap = parse_snapshot(buf.data(), (uint32_t)buf.size());

        /* should have FIRE_BLOCKED with reason reloading */
        bool found_blocked = false;
        for (uint32_t i = 0; i < snap.header->event_count; ++i) {
            if (snap.events[i].type == AX_EVT_FIRE_BLOCKED) {
                found_blocked = true;
                CHECK(snap.events[i].value == AX_FIRE_BLOCKED_RELOADING,
                      "expected reason reloading (1), got %d", snap.events[i].value);
            }
        }
        CHECK(found_blocked, "FIRE_BLOCKED event not found during reload");
    }

    /* ── Phase 5: step remaining ticks for reload to complete ─────── */
    /*
     * Reload started at the tick we submitted RELOAD.
     * Timer = 30 ticks. Timer decrements once per tick AFTER actions.
     *
     * Tick where RELOAD submitted: timer set to 30, then decremented to 29
     * Next tick (phase 4):         decremented to 28
     * We need 28 more ticks for it to reach 0.
     */
    for (uint32_t i = 0; i < 28; ++i) {
        tick++;
        ax_step_ticks(core, 1);
    }

    /* check the RELOAD_DONE event on the completion tick */
    {
        auto buf = take_snapshot(core);
        parsed_snapshot snap = parse_snapshot(buf.data(), (uint32_t)buf.size());

        bool found_reload_done = false;
        for (uint32_t i = 0; i < snap.header->event_count; ++i) {
            if (snap.events[i].type == AX_EVT_RELOAD_DONE) {
                found_reload_done = true;
                CHECK(snap.events[i].a == 1, "RELOAD_DONE actor should be 1, got %u",
                      snap.events[i].a);
                CHECK(snap.events[i].b == 0, "RELOAD_DONE weapon_slot should be 0, got %u",
                      snap.events[i].b);
                CHECK(snap.events[i].value == 12, "RELOAD_DONE should load 12 rounds, got %d",
                      snap.events[i].value);
            }
        }
        CHECK(found_reload_done, "RELOAD_DONE event not found after 30 ticks");

        /* weapon state should show full mag, not reloading */
        CHECK(snap.weapon != nullptr, "weapon block missing");
        if (snap.weapon) {
            CHECK(snap.weapon->ammo_in_mag == 12, "mag should be 12 after reload, got %d",
                  snap.weapon->ammo_in_mag);
            CHECK(snap.weapon->ammo_reserve == 36, "reserve should be 36 (48-12), got %d",
                  snap.weapon->ammo_reserve);
            CHECK((snap.weapon->weapon_flags & AX_WPN_FLAG_RELOADING) == 0,
                  "weapon should NOT have RELOADING flag after completion");
        }
    }

    /* ── Phase 6: fire after reload → should succeed ──────────────── */
    tick++;
    {
        ax_action_v1 fire = {};
        fire.tick     = tick;
        fire.actor_id = 1;
        fire.type     = AX_ACT_FIRE_ONCE;
        fire.u.fire_once.weapon_slot = 0;

        submit_action(core, fire);
        ax_step_ticks(core, 1);

        auto buf = take_snapshot(core);
        parsed_snapshot snap = parse_snapshot(buf.data(), (uint32_t)buf.size());

        /* should have DAMAGE_DEALT, not FIRE_BLOCKED */
        bool found_damage = false;
        bool found_blocked = false;
        for (uint32_t i = 0; i < snap.header->event_count; ++i) {
            if (snap.events[i].type == AX_EVT_DAMAGE_DEALT)  found_damage = true;
            if (snap.events[i].type == AX_EVT_FIRE_BLOCKED)  found_blocked = true;
        }
        CHECK(found_damage, "should have DAMAGE_DEALT after reload");
        CHECK(!found_blocked, "should NOT have FIRE_BLOCKED after reload");

        /* ammo should be 11 */
        if (snap.weapon) {
            CHECK(snap.weapon->ammo_in_mag == 11, "mag should be 11 after post-reload fire, got %d",
                  snap.weapon->ammo_in_mag);
        }
    }

    ax_unload_content(core);
    ax_destroy(core);
    printf("  done\n");
}

/* ══════════════════════════════════════════════════════════════════════
 * Test: deterministic replay
 * COMBAT_A1 acceptance criteria #2
 * ══════════════════════════════════════════════════════════════════ */

static void test_deterministic_replay(void) {
    printf("test_deterministic_replay\n");

    /*
     * Build a scripted action sequence: mix of fire, move, reload.
     * This exercises multiple action types to increase coverage.
     */
    struct scripted_action {
        uint64_t tick;
        uint32_t type;
        float    f1, f2;       /* move x/y, look yaw/pitch, or unused */
        uint32_t weapon_slot;  /* for fire/reload */
    };

    const scripted_action script[] = {
        /* tick  type               f1     f2     slot */
        {  1,    AX_ACT_MOVE_INTENT,  0.0f,  1.0f,  0 },   /* move forward  */
        {  2,    AX_ACT_FIRE_ONCE,    0.0f,  0.0f,  0 },   /* fire           */
        {  3,    AX_ACT_FIRE_ONCE,    0.0f,  0.0f,  0 },   /* fire           */
        {  4,    AX_ACT_LOOK_INTENT,  0.5f,  0.0f,  0 },   /* look right     */
        {  5,    AX_ACT_FIRE_ONCE,    0.0f,  0.0f,  0 },   /* fire           */
        {  6,    AX_ACT_MOVE_INTENT, -1.0f,  0.0f,  0 },   /* strafe left    */
        {  7,    AX_ACT_FIRE_ONCE,    0.0f,  0.0f,  0 },   /* fire           */
        {  8,    AX_ACT_FIRE_ONCE,    0.0f,  0.0f,  0 },   /* fire (target 100 dies here) */
        {  9,    AX_ACT_FIRE_ONCE,    0.0f,  0.0f,  0 },   /* fire target 101 */
        { 10,    AX_ACT_RELOAD,       0.0f,  0.0f,  0 },   /* reload (ignored: mag not empty) */
        { 11,    AX_ACT_FIRE_ONCE,    0.0f,  0.0f,  0 },   /* fire           */
        { 12,    AX_ACT_FIRE_ONCE,    0.0f,  0.0f,  0 },   /* fire           */
    };
    const uint32_t script_len = sizeof(script) / sizeof(script[0]);
    const uint64_t total_ticks = 12;

    /*
     * Run the sequence on two independent core instances and
     * collect final snapshots + accumulated events.
     */
    struct run_result {
        std::vector<uint8_t> final_snapshot;
        int32_t total_damage;
        int32_t destroy_count;
        int32_t fire_blocked_count;

        /* per-tick event log for sequence comparison */
        struct tick_events {
            uint64_t tick;
            std::vector<ax_snapshot_event_v1> events;
        };
        std::vector<tick_events> event_log;
    };

    run_result runs[2] = {};

    for (int run = 0; run < 2; ++run) {
        ax_core* core = create_and_load("content/");
        CHECK(core != nullptr, "run %d: core creation failed", run);
        if (!core) return;

        runs[run].total_damage      = 0;
        runs[run].destroy_count     = 0;
        runs[run].fire_blocked_count = 0;

        for (uint64_t t = 1; t <= total_ticks; ++t) {
            /* find and submit any actions for this tick */
            for (uint32_t s = 0; s < script_len; ++s) {
                if (script[s].tick != t) continue;

                ax_action_v1 act = {};
                act.tick     = t;
                act.actor_id = 1;
                act.type     = script[s].type;

                switch (script[s].type) {
                    case AX_ACT_MOVE_INTENT:
                        act.u.move.x = script[s].f1;
                        act.u.move.y = script[s].f2;
                        break;
                    case AX_ACT_LOOK_INTENT:
                        act.u.look.yaw   = script[s].f1;
                        act.u.look.pitch = script[s].f2;
                        break;
                    case AX_ACT_FIRE_ONCE:
                        act.u.fire_once.weapon_slot = script[s].weapon_slot;
                        break;
                    case AX_ACT_RELOAD:
                        act.u.reload.weapon_slot = script[s].weapon_slot;
                        break;
                    default:
                        break;
                }

                submit_action(core, act);
            }

            ax_step_ticks(core, 1);

            /* collect events */
            auto buf = take_snapshot(core);
            parsed_snapshot snap = parse_snapshot(buf.data(), (uint32_t)buf.size());

            run_result::tick_events te;
            te.tick = t;
            for (uint32_t i = 0; i < snap.header->event_count; ++i) {
                const ax_snapshot_event_v1* evt = &snap.events[i];
                te.events.push_back(*evt);

                if (evt->type == AX_EVT_DAMAGE_DEALT)  runs[run].total_damage += evt->value;
                if (evt->type == AX_EVT_TARGET_DESTROY) runs[run].destroy_count++;
                if (evt->type == AX_EVT_FIRE_BLOCKED)   runs[run].fire_blocked_count++;
            }
            runs[run].event_log.push_back(te);
        }

        /* take final snapshot */
        runs[run].final_snapshot = take_snapshot(core);

        ax_unload_content(core);
        ax_destroy(core);
    }

    /* ── Compare results ──────────────────────────────────────────── */

    /* aggregate counters */
    CHECK(runs[0].total_damage == runs[1].total_damage,
          "total_damage mismatch: run0=%d run1=%d",
          runs[0].total_damage, runs[1].total_damage);
    CHECK(runs[0].destroy_count == runs[1].destroy_count,
          "destroy_count mismatch: run0=%d run1=%d",
          runs[0].destroy_count, runs[1].destroy_count);
    CHECK(runs[0].fire_blocked_count == runs[1].fire_blocked_count,
          "fire_blocked_count mismatch: run0=%d run1=%d",
          runs[0].fire_blocked_count, runs[1].fire_blocked_count);

    /* per-tick event sequence comparison */
    CHECK(runs[0].event_log.size() == runs[1].event_log.size(),
          "event_log size mismatch: run0=%zu run1=%zu",
          runs[0].event_log.size(), runs[1].event_log.size());

    size_t log_len = runs[0].event_log.size() < runs[1].event_log.size()
                   ? runs[0].event_log.size() : runs[1].event_log.size();

    for (size_t t = 0; t < log_len; ++t) {
        const auto& e0 = runs[0].event_log[t];
        const auto& e1 = runs[1].event_log[t];

        CHECK(e0.events.size() == e1.events.size(),
              "tick %llu: event count mismatch: run0=%zu run1=%zu",
              (unsigned long long)e0.tick, e0.events.size(), e1.events.size());

        size_t evt_len = e0.events.size() < e1.events.size()
                       ? e0.events.size() : e1.events.size();

        for (size_t i = 0; i < evt_len; ++i) {
            CHECK(e0.events[i].type == e1.events[i].type,
                  "tick %llu event[%zu]: type mismatch %u vs %u",
                  (unsigned long long)e0.tick, i, e0.events[i].type, e1.events[i].type);
            CHECK(e0.events[i].a == e1.events[i].a,
                  "tick %llu event[%zu]: actor mismatch %u vs %u",
                  (unsigned long long)e0.tick, i, e0.events[i].a, e1.events[i].a);
            CHECK(e0.events[i].b == e1.events[i].b,
                  "tick %llu event[%zu]: target mismatch %u vs %u",
                  (unsigned long long)e0.tick, i, e0.events[i].b, e1.events[i].b);
            CHECK(e0.events[i].value == e1.events[i].value,
                  "tick %llu event[%zu]: value mismatch %d vs %d",
                  (unsigned long long)e0.tick, i, e0.events[i].value, e1.events[i].value);
        }
    }

    /* final snapshot: compare logic-relevant fields */
    {
        parsed_snapshot s0 = parse_snapshot(runs[0].final_snapshot.data(),
                                           (uint32_t)runs[0].final_snapshot.size());
        parsed_snapshot s1 = parse_snapshot(runs[1].final_snapshot.data(),
                                           (uint32_t)runs[1].final_snapshot.size());

        CHECK(s0.header->tick == s1.header->tick,
              "final tick mismatch: %llu vs %llu",
              (unsigned long long)s0.header->tick,
              (unsigned long long)s1.header->tick);

        CHECK(s0.header->entity_count == s1.header->entity_count,
              "entity_count mismatch: %u vs %u",
              s0.header->entity_count, s1.header->entity_count);

        uint32_t ent_count = s0.header->entity_count < s1.header->entity_count
                           ? s0.header->entity_count : s1.header->entity_count;

        for (uint32_t i = 0; i < ent_count; ++i) {
            CHECK(s0.entities[i].id == s1.entities[i].id,
                  "entity[%u] id mismatch", i);
            CHECK(s0.entities[i].hp == s1.entities[i].hp,
                  "entity[%u] hp mismatch: %d vs %d",
                  i, s0.entities[i].hp, s1.entities[i].hp);
            CHECK(s0.entities[i].state_flags == s1.entities[i].state_flags,
                  "entity[%u] state_flags mismatch: 0x%x vs 0x%x",
                  i, s0.entities[i].state_flags, s1.entities[i].state_flags);
        }

        /* weapon state */
        if (s0.weapon && s1.weapon) {
            CHECK(s0.weapon->ammo_in_mag == s1.weapon->ammo_in_mag,
                  "ammo_in_mag mismatch: %d vs %d",
                  s0.weapon->ammo_in_mag, s1.weapon->ammo_in_mag);
            CHECK(s0.weapon->ammo_reserve == s1.weapon->ammo_reserve,
                  "ammo_reserve mismatch: %d vs %d",
                  s0.weapon->ammo_reserve, s1.weapon->ammo_reserve);
            CHECK(s0.weapon->weapon_flags == s1.weapon->weapon_flags,
                  "weapon_flags mismatch: 0x%x vs 0x%x",
                  s0.weapon->weapon_flags, s1.weapon->weapon_flags);
        }
    }

    printf("  done\n");
}

/* ══════════════════════════════════════════════════════════════════════
 * Test: error paths
 * Validates structural validation, lifecycle enforcement, and
 * buffer-too-small behavior across the ABI surface.
 * ══════════════════════════════════════════════════════════════════ */

static void test_error_paths(void) {
    printf("test_error_paths\n");

    /* ── ax_create error cases ────────────────────────────────────── */

    /* NULL params */
    {
        ax_core* core = nullptr;
        ax_result r = ax_create(nullptr, &core);
        CHECK_ERR(r, AX_ERR_INVALID_ARG);
        CHECK(core == nullptr, "core should remain NULL on failure");
    }

    /* NULL out_core */
    {
        ax_create_params_v1 params = {};
        params.version    = 1;
        params.size_bytes = sizeof(params);
        params.abi_major  = AX_ABI_MAJOR;
        params.abi_minor  = AX_ABI_MINOR;

        ax_result r = ax_create(&params, nullptr);
        CHECK_ERR(r, AX_ERR_INVALID_ARG);
    }

    /* wrong ABI major */
    {
        ax_create_params_v1 params = {};
        params.version    = 1;
        params.size_bytes = sizeof(params);
        params.abi_major  = AX_ABI_MAJOR + 99;
        params.abi_minor  = AX_ABI_MINOR;

        ax_core* core = nullptr;
        ax_result r = ax_create(&params, &core);
        CHECK_ERR(r, AX_ERR_UNSUPPORTED);
        CHECK(core == nullptr, "core should remain NULL on ABI mismatch");
    }

    /* bad struct version */
    {
        ax_create_params_v1 params = {};
        params.version    = 255;
        params.size_bytes = sizeof(params);
        params.abi_major  = AX_ABI_MAJOR;
        params.abi_minor  = AX_ABI_MINOR;

        ax_core* core = nullptr;
        ax_result r = ax_create(&params, &core);
        CHECK_ERR(r, AX_ERR_UNSUPPORTED);
    }

    /* size_bytes too small */
    {
        ax_create_params_v1 params = {};
        params.version    = 1;
        params.size_bytes = 4;  /* way too small */
        params.abi_major  = AX_ABI_MAJOR;
        params.abi_minor  = AX_ABI_MINOR;

        ax_core* core = nullptr;
        ax_result r = ax_create(&params, &core);
        CHECK_ERR(r, AX_ERR_INVALID_ARG);
    }

    /* ── lifecycle enforcement ────────────────────────────────────── */

    /* step before content load */
    {
        ax_create_params_v1 params = {};
        params.version    = 1;
        params.size_bytes = sizeof(params);
        params.abi_major  = AX_ABI_MAJOR;
        params.abi_minor  = AX_ABI_MINOR;

        ax_core* core = nullptr;
        ax_create(&params, &core);

        ax_result r = ax_step_ticks(core, 1);
        CHECK_ERR(r, AX_ERR_BAD_STATE);

        ax_destroy(core);
    }

    /* submit actions before content load */
    {
        ax_create_params_v1 params = {};
        params.version    = 1;
        params.size_bytes = sizeof(params);
        params.abi_major  = AX_ABI_MAJOR;
        params.abi_minor  = AX_ABI_MINOR;

        ax_core* core = nullptr;
        ax_create(&params, &core);

        ax_action_batch_v1 batch = {};
        batch.version    = 1;
        batch.size_bytes = sizeof(batch);
        batch.count      = 0;
        batch.actions    = nullptr;

        ax_result r = ax_submit_actions(core, &batch);
        CHECK_ERR(r, AX_ERR_BAD_STATE);

        ax_destroy(core);
    }

    /* snapshot before content load */
    {
        ax_create_params_v1 params = {};
        params.version    = 1;
        params.size_bytes = sizeof(params);
        params.abi_major  = AX_ABI_MAJOR;
        params.abi_minor  = AX_ABI_MINOR;

        ax_core* core = nullptr;
        ax_create(&params, &core);

        uint32_t size = 0;
        ax_result r = ax_get_snapshot_bytes(core, nullptr, 0, &size);
        CHECK_ERR(r, AX_ERR_BAD_STATE);

        ax_destroy(core);
    }

    /* double content load */
    {
        ax_core* core = create_and_load("content/");
        CHECK(core != nullptr, "setup failed");
        if (core) {
            ax_content_load_params_v1 content = {};
            content.version    = 1;
            content.size_bytes = sizeof(content);
            content.root_path  = "content/";

            ax_result r = ax_load_content(core, &content);
            CHECK_ERR(r, AX_ERR_BAD_STATE);

            ax_unload_content(core);
            ax_destroy(core);
        }
    }

    /* save before content load */
    {
        ax_create_params_v1 params = {};
        params.version    = 1;
        params.size_bytes = sizeof(params);
        params.abi_major  = AX_ABI_MAJOR;
        params.abi_minor  = AX_ABI_MINOR;

        ax_core* core = nullptr;
        ax_create(&params, &core);

        uint32_t size = 0;
        ax_result r = ax_save_bytes(core, nullptr, 0, &size);
        CHECK_ERR(r, AX_ERR_BAD_STATE);

        ax_destroy(core);
    }

    /* load save before content load */
    {
        ax_create_params_v1 params = {};
        params.version    = 1;
        params.size_bytes = sizeof(params);
        params.abi_major  = AX_ABI_MAJOR;
        params.abi_minor  = AX_ABI_MINOR;

        ax_core* core = nullptr;
        ax_create(&params, &core);

        uint8_t dummy = 0;
        ax_result r = ax_load_save_bytes(core, &dummy, 1);
        CHECK_ERR(r, AX_ERR_BAD_STATE);

        ax_destroy(core);
    }

    /* ── ax_load_content validation ───────────────────────────────── */

    /* NULL root_path */
    {
        ax_create_params_v1 params = {};
        params.version    = 1;
        params.size_bytes = sizeof(params);
        params.abi_major  = AX_ABI_MAJOR;
        params.abi_minor  = AX_ABI_MINOR;

        ax_core* core = nullptr;
        ax_create(&params, &core);

        ax_content_load_params_v1 content = {};
        content.version    = 1;
        content.size_bytes = sizeof(content);
        content.root_path  = nullptr;

        ax_result r = ax_load_content(core, &content);
        CHECK_ERR(r, AX_ERR_INVALID_ARG);

        ax_destroy(core);
    }

    /* empty root_path */
    {
        ax_create_params_v1 params = {};
        params.version    = 1;
        params.size_bytes = sizeof(params);
        params.abi_major  = AX_ABI_MAJOR;
        params.abi_minor  = AX_ABI_MINOR;

        ax_core* core = nullptr;
        ax_create(&params, &core);

        ax_content_load_params_v1 content = {};
        content.version    = 1;
        content.size_bytes = sizeof(content);
        content.root_path  = "";

        ax_result r = ax_load_content(core, &content);
        CHECK_ERR(r, AX_ERR_INVALID_ARG);

        ax_destroy(core);
    }

    /* ── action submission validation ─────────────────────────────── */

    /* bad batch version */
    {
        ax_core* core = create_and_load("content/");
        CHECK(core != nullptr, "setup failed");
        if (core) {
            ax_action_batch_v1 batch = {};
            batch.version    = 99;
            batch.size_bytes = sizeof(batch);
            batch.count      = 0;
            batch.actions    = nullptr;

            ax_result r = ax_submit_actions(core, &batch);
            CHECK_ERR(r, AX_ERR_UNSUPPORTED);

            ax_unload_content(core);
            ax_destroy(core);
        }
    }

    /* count > 0 but actions is NULL */
    {
        ax_core* core = create_and_load("content/");
        CHECK(core != nullptr, "setup failed");
        if (core) {
            ax_action_batch_v1 batch = {};
            batch.version    = 1;
            batch.size_bytes = sizeof(batch);
            batch.count      = 5;
            batch.actions    = nullptr;

            ax_result r = ax_submit_actions(core, &batch);
            CHECK_ERR(r, AX_ERR_INVALID_ARG);

            ax_unload_content(core);
            ax_destroy(core);
        }
    }

    /* unknown action type */
    {
        ax_core* core = create_and_load("content/");
        CHECK(core != nullptr, "setup failed");
        if (core) {
            ax_action_v1 act = {};
            act.tick     = 1;
            act.actor_id = 1;
            act.type     = 999;

            ax_action_batch_v1 batch = {};
            batch.version    = 1;
            batch.size_bytes = sizeof(batch);
            batch.count      = 1;
            batch.actions    = &act;

            ax_result r = ax_submit_actions(core, &batch);
            CHECK_ERR(r, AX_ERR_INVALID_ARG);

            ax_unload_content(core);
            ax_destroy(core);
        }
    }

    /* NaN in move action (structural validation) */
    {
        ax_core* core = create_and_load("content/");
        CHECK(core != nullptr, "setup failed");
        if (core) {
            ax_action_v1 act = {};
            act.tick     = 1;
            act.actor_id = 1;
            act.type     = AX_ACT_MOVE_INTENT;
            act.u.move.x = NAN;  /* NaN */
            act.u.move.y = 1.0f;

            ax_action_batch_v1 batch = {};
            batch.version    = 1;
            batch.size_bytes = sizeof(batch);
            batch.count      = 1;
            batch.actions    = &act;

            ax_result r = ax_submit_actions(core, &batch);
            CHECK_ERR(r, AX_ERR_INVALID_ARG);

            ax_unload_content(core);
            ax_destroy(core);
        }
    }

    /* ── snapshot buffer-too-small ─────────────────────────────────── */
    {
        ax_core* core = create_and_load("content/");
        CHECK(core != nullptr, "setup failed");
        if (core) {
            /* query required size */
            uint32_t required = 0;
            ax_result r = ax_get_snapshot_bytes(core, nullptr, 0, &required);
            CHECK_OK(r);
            CHECK(required > 0, "required size should be > 0, got %u", required);

            /* provide a buffer that's too small */
            std::vector<uint8_t> small_buf(4);  /* way too small */
            uint32_t written = 0;
            r = ax_get_snapshot_bytes(core, small_buf.data(), 4, &written);
            CHECK_ERR(r, AX_ERR_BUFFER_TOO_SMALL);
            CHECK(written == required,
                  "out_size_bytes should be required size %u even on error, got %u",
                  required, written);

            ax_unload_content(core);
            ax_destroy(core);
        }
    }

    /* ── diagnostics with NULL ────────────────────────────────────── */
    {
        ax_result r = ax_get_diagnostics(nullptr, nullptr);
        CHECK_ERR(r, AX_ERR_INVALID_ARG);
    }

    /* ── ax_get_last_error after failure should be non-empty ──────── */
    {
        ax_result r = ax_create(nullptr, nullptr);
        CHECK_ERR(r, AX_ERR_INVALID_ARG);
        const char* err = ax_get_last_error();
        CHECK(err != nullptr, "last error should not be NULL");
        CHECK(err[0] != '\0', "last error should be non-empty after failure");
    }

    printf("  done\n");
}

/* ══════════════════════════════════════════════════════════════════════
 * Main — run all tests
 * ══════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("=== Axiom Headless Shell (A1 Tests) ===\n\n");

    test_basic_fire_and_damage();
    test_reload_cycle();
    test_deterministic_replay();
    test_error_paths();

    printf("\n=== Results: %d passed, %d failed, %d total ===\n",
           g_tests_passed, g_tests_failed, g_tests_run);

    return g_tests_failed > 0 ? 1 : 0;
}