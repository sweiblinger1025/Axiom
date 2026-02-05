/*
 * world.cpp - Axiom World implementation
 *
 * TASK-001: dimensions + tick advancement
 * TASK-002: spatial grid allocation (terrain + occupancy)
 * TASK-003: command pipeline (tick-boundary processing)
 *
 * The step() loop now follows COMMAND_MODEL.md:
 *   1. Clear previous results
 *   2. Process queued commands (validate → apply → emit result)
 *   3. (Future: simulation systems)
 *   4. Advance tick
 *
 * See COMMAND_MODEL.md for full lifecycle semantics.
 */

#include "world.h"

namespace axiom {

World::World(uint32_t width, uint32_t height)
    : m_width(width)
    , m_height(height)
    , m_cellCount(width * height)
    , m_tick(0)
    , m_terrain(m_cellCount, 0)
    , m_occupancy(m_cellCount, 0)
    , m_nextCommandId(1)
{
}

uint64_t World::submitCommand(PendingCommand cmd)
{
    uint64_t id = m_nextCommandId++;
    cmd.id = id;
    m_pendingCommands.push_back(cmd);
    return id;
}

void World::processCommand(const PendingCommand& cmd)
{
    CommandResult result{};
    result.commandId   = cmd.id;
    result.tickApplied = m_tick;
    result.type        = cmd.type;

    /*
     * State-dependent validation for AX_CMD_DEBUG_SET_CELL_U8.
     *
     * Structural validation (version, type, null) already happened
     * at the C API layer during submission. Here we only check
     * things that depend on world state.
     *
     * See COMMAND_MODEL.md: "Validation logic must be deterministic,
     * side-effect free, based only on current world state and
     * command parameters."
     */

    /* Bounds check */
    if (cmd.x >= m_width || cmd.y >= m_height)
    {
        result.accepted = 0;
        result.reason   = 1; /* AX_CMD_REJECT_INVALID_COORDS */
        m_results.push_back(result);
        return;
    }

    /* Channel check */
    const uint8_t SNAP_TERRAIN   = 2; /* AX_SNAP_TERRAIN   */
    const uint8_t SNAP_OCCUPANCY = 3; /* AX_SNAP_OCCUPANCY */

    if (cmd.channel != SNAP_TERRAIN && cmd.channel != SNAP_OCCUPANCY)
    {
        result.accepted = 0;
        result.reason   = 2; /* AX_CMD_REJECT_INVALID_CHANNEL */
        m_results.push_back(result);
        return;
    }

    /*
     * Apply mutation.
     *
     * Linear index per SPATIAL_MODEL.md:
     *   index = y * width + x
     *
     * Later commands in the same tick see this effect,
     * per COMMAND_MODEL.md sequential semantics.
     */
    uint32_t index = cmd.y * m_width + cmd.x;

    if (cmd.channel == SNAP_TERRAIN)
    {
        m_terrain[index] = cmd.value;
    }
    else /* SNAP_OCCUPANCY */
    {
        m_occupancy[index] = cmd.value;
    }

    result.accepted = 1;
    result.reason   = 0; /* AX_CMD_REJECT_NONE */
    m_results.push_back(result);
}

void World::step(uint32_t count)
{
    /*
     * Advance the simulation by 'count' ticks.
     *
     * Per COMMAND_MODEL.md and TASK-003 spec:
     *   - Results from the previous tick are cleared
     *   - Queued commands are processed sequentially
     *   - Each command sees the effects of prior commands
     *   - Tick counter advances after processing
     *
     * See ARCHITECTURE_OVERVIEW.md: "Simulation is composed of
     * modular systems updated in a fixed order per tick."
     */
    for (uint32_t i = 0; i < count; ++i)
    {
        /* 1. Clear results from previous tick */
        m_results.clear();

        /* 2. Process queued commands at tick boundary.
         *    Move the queue to a local to allow commands
         *    submitted during processing (not expected in v0,
         *    but defensive). */
        std::vector<PendingCommand> batch;
        batch.swap(m_pendingCommands);

        for (const auto& cmd : batch)
        {
            processCommand(cmd);
        }

        /* 3. (Future: simulation systems — thermal, gas, liquid) */

        /* 4. Advance tick */
        ++m_tick;
    }
}

} // namespace axiom