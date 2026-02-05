/*
* world.cpp - Axiom World implementation
 *
 * TASK-001: dimensions + tick advancement
 * TASK-002: spatial grid allocation (terrain + occupancy)
 *
 * The World constructor allocates per-cell SoA arrays for
 * terrain and occupancy, default-initialized to zero.
 *
 * See SPATIAL_MODEL.md for indexing rules.
 * See DECISIONS.md D006 for SoA layout rationale.
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
    {
    }

    void World::step(uint32_t count)
    {
        /*
         * Advance the simulation by 'count' ticks.
         *
         * The per-tick loop exists so that future system updates
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
            // No simulation systems yet (TASK-002).
            ++m_tick;
        }
    }

} // namespace axiom