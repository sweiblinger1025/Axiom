#include "ax_abi.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstring>

static void log_cb(void*, int level, const char* msg) {
    std::printf("[LOG %d] %s\n", level, msg ? msg : "(null)");
}

int main() {
    ax_create_params_v1 cp{};
    cp.version = 1;
    cp.size_bytes = (uint32_t)sizeof(ax_create_params_v1);
    cp.log_fn = &log_cb;
    cp.log_user = nullptr;

    ax_core* core = nullptr;
    ax_result r = ax_create(&cp, &core);
    if (r != AX_OK) {
        std::printf("ax_create failed: %d\n", (int)r);
        return 1;
    }

    // Submit a simple "fire once" action for next tick
    ax_action_v1 acts[1]{};
    acts[0].type = AX_ACT_FIRE_ONCE;
    acts[0].u.fire_once.weapon_slot = 0;

    ax_action_batch_v1 batch{};
    batch.version = 1;
    batch.size_bytes = (uint32_t)sizeof(ax_action_batch_v1);
    batch.action_count = 1;
    batch.action_stride_bytes = (uint32_t)sizeof(ax_action_v1);
    batch.actions = acts;

    r = ax_submit_actions_next_tick(core, &batch);
    if (r != AX_OK) {
        std::printf("submit failed: %s\n", ax_get_last_error(core));
    }

    r = ax_step_ticks(core, 1);
    if (r != AX_OK) {
        std::printf("step failed: %s\n", ax_get_last_error(core));
    }

    // Snapshot query
    uint32_t need = 0;
    r = ax_get_snapshot_bytes(core, NULL, 0, &need);
    if (r != AX_OK) {
        std::printf("snapshot size query failed: %s\n", ax_get_last_error(core));
        ax_destroy(core);
        return 1;
    }

    std::vector<uint8_t> buf(need);
    r = ax_get_snapshot_bytes(core, buf.data(), (uint32_t)buf.size(), &need);
    if (r != AX_OK) {
        std::printf("snapshot copy failed: %s\n", ax_get_last_error(core));
        ax_destroy(core);
        return 1;
    }

    const ax_snapshot_header_v1* h = (const ax_snapshot_header_v1*)buf.data();
    std::printf("Snapshot tick=%llu entities=%u events=%u size=%u\n",
                (unsigned long long)h->tick, h->entity_count, h->event_count, h->size_bytes);

    ax_destroy(core);
    return 0;
}
