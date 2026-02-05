/*
 * world.h - Axiom World (simulation truth)
 *
 * The World is the authoritative runtime object that owns all
 * simulation state.
 *
 * TASK-001: dimensions + tick counter
 * TASK-002: spatial grid (terrain + occupancy SoA channels)
 * TASK-003: command pipeline (submission, tick-boundary processing, results)
 *
 * This is internal engine code. External access occurs only
 * through the C API (ax_api.h).
 *
 * Governing docs:
 *   ARCHITECTURE_OVERVIEW.md  - "C++ Owns Truth"
 *   COMMAND_MODEL.md          - command lifecycle, validation, results
 *   SPATIAL_MODEL.md          - coordinate system, indexing, bounds
 *   DECISIONS.md D006         - SoA for per-cell fields
 *   TRUTH_VS_PRESENTATION.md  - no presentation concerns here
 */

#ifndef AXIOM_CORE_WORLD_H
#define AXIOM_CORE_WORLD_H

#include <cstdint>
#include <vector>

namespace axiom {

/*
 * Internal command representation.
 *
 * The C API layer translates from ax_command_v1 into this format
 * at submission time. This keeps the core independent of the
 * public API header.
 *
 * v0: fields are tailored to SET_CELL_U8. Future command types
 * will require extending this (variant, union, or refactor).
 */
struct PendingCommand
{
    uint64_t id;        /* assigned by World at submission        */
    uint32_t type;      /* ax_command_type value                  */
    uint32_t x;         /* cell x coordinate                     */
    uint32_t y;         /* cell y coordinate                     */
    uint8_t  channel;   /* target snapshot channel                */
    uint8_t  value;     /* value to write                        */
};

/*
 * Internal result representation.
 *
 * The C API layer translates from this into ax_command_result_v1
 * when the viewer reads results.
 */
struct CommandResult
{
    uint64_t commandId;     /* correlates with PendingCommand::id */
    uint64_t tickApplied;   /* tick when processed                */
    uint32_t type;          /* ax_command_type value               */
    uint8_t  accepted;      /* 1 = applied, 0 = rejected          */
    uint8_t  reason;        /* ax_command_reject_reason value      */
};

class World
{
public:
    /*
     * Construct a world with the given dimensions.
     *
     * Preconditions (enforced by caller - the C API layer):
     *   width > 0
     *   height > 0
     *   width * height does not overflow uint32_t
     *
     * Initial state:
     *   tick = 0
     *   all terrain cells = 0 (empty)
     *   all occupancy cells = 0 (unoccupied)
     *   command ID counter = 1
     *   command queue and results empty
     *
     * Dimensions and cell count are immutable after construction.
     * See SPATIAL_MODEL.md: "Bounds are defined at world creation time."
     */
    World(uint32_t width, uint32_t height);

    /* Non-copyable: World is a unique identity.
     * Copying would create divergent truth. */
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    /* Movable: allows placement in containers if needed. */
    World(World&&) noexcept = default;
    World& operator=(World&&) noexcept = default;

    ~World() = default;

    /* --- Tick advancement ---------------------------------
     *
     * Advances the simulation by the given number of ticks.
     *
     * For each tick:
     *   1. Clear previous tick's results
     *   2. Process queued commands (validate → apply → emit result)
     *   3. (Future: run simulation systems)
     *   4. Increment tick counter
     *
     * Precondition: count > 0 (enforced by caller).
     * See DECISIONS.md D001: "10 Hz fixed simulation tick."
     * See COMMAND_MODEL.md: "Commands are processed at tick boundaries."
     */
    void step(uint32_t count);

    /* --- State queries ------------------------------------ */

    uint64_t tick() const       { return m_tick; }
    uint32_t width() const      { return m_width; }
    uint32_t height() const     { return m_height; }
    uint32_t cellCount() const  { return m_cellCount; }

    /* --- Channel data access (read-only) ------------------
     *
     * Returns pointers to contiguous SoA arrays.
     * Length of each array = m_cellCount.
     * Indexed by: index = y * width + x
     *
     * See SPATIAL_MODEL.md: canonical linear index.
     * See DECISIONS.md D006: SoA for per-cell fields.
     */
    const uint8_t* terrain() const   { return m_terrain.data(); }
    const uint8_t* occupancy() const { return m_occupancy.data(); }

    /* --- Command submission (TASK-003) --------------------
     *
     * Enqueues a command for processing at the next tick boundary.
     * Assigns a monotonic command ID.
     *
     * The caller (C API layer) has already performed structural
     * validation (version, type, null checks). This method
     * only assigns the ID and enqueues.
     *
     * Returns the engine-assigned command ID (always > 0).
     */
    uint64_t submitCommand(PendingCommand cmd);

    /* --- Command results (TASK-003) -----------------------
     *
     * Results from the most recent tick's command processing.
     * Idempotent read — results persist until the next step().
     *
     * See COMMAND_MODEL.md: "Each processed command produces
     * exactly one result."
     */
    const std::vector<CommandResult>& results() const { return m_results; }

private:
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_cellCount;
    uint64_t m_tick = 0;

    /* Per-cell SoA channels (TASK-002). */
    std::vector<uint8_t> m_terrain;
    std::vector<uint8_t> m_occupancy;

    /* Command pipeline (TASK-003). */
    uint64_t                   m_nextCommandId = 1;
    std::vector<PendingCommand> m_pendingCommands;
    std::vector<CommandResult>  m_results;

    /*
     * Process a single command during tick-boundary execution.
     *
     * Performs state-dependent validation, then either:
     *   - applies the mutation and emits an accepted result, or
     *   - emits a rejected result with reason
     *
     * Called sequentially for each queued command in submission order.
     */
    void processCommand(const PendingCommand& cmd);
};

} // namespace axiom

#endif // AXIOM_CORE_WORLD_H