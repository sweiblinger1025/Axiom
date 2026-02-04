/*
 * world.cpp - Axiom World implementation
 *
 * For TASK-001, the World is minimal:
 *   - stores dimensions (immutable)
 *   - advances a tick counter
 *
 * Future milestones will add simulation system updates
 * inside the step loop.
 */

#include "world.h"

namespace axiom {
    World::World(uint32_t width, uint32_t height)
        : m_width(width)
        , m_height(height)
        , m_tick(0)
{
}

void World::step(uint32_t count)
{
    /*
     * Advance the simulation by 'count' ticks.
     *
     * The per-pick loop exists so that future system updates
     * can be inserted here naturally:
     *
     *   for (...)
     *   {
     *       applyCommands();
     *       updateDevices();
     *       updateLiquids();
     *       updateGases();
     *       updateThermal();
     *       generateEvents();
     *       ++m_tick;
     *   }
     *
     * See ARCHITECTURE_OVERVIEW.md: "Simulation is composed of
     * modular systems updated in a fixed order per tick."
     */
    for (uint32_t i = 0; i < count; ++i)
    {
        // No simulation systems yet (TASK-001).
        ++m_tick;
    }
}
} // namespace axiom