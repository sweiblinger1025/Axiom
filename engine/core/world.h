/*
 * world.h - Axiom World (simulation truth)
 *
 * The World is the authoritative runtime object that owns all
 * simulation state.
 *
 * TASK-001: dimensions + tick counter
 * TASK-002: spatial grid (terrain + occupancy SoA channels)
 *
 * This is internal engine code. External access occurs only
 * through the C API (ax_api.h).
 *
 * Governing docs:
 *   ARCHITECTURE_OVERVIEW.md  - "C++ Owns Truth"
 *   SPATIAL_MODEL.md          - coordinate system, indexing, bounds
 *   DECISIONS.md D006         - SoA for per-cell fields
 *   TRUTH_VS_PRESENTATION.md  - no presentation concerns here
 */

#ifndef AXIOM_CORE_WORLD_H
#define AXIOM_CORE_WORLD_H

#include <cstdint>
#include <vector>

namespace axiom {

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
     * Each tick increments the counter by 1.
     *
     * Precondition: count > 0 (enforced by caller).
     * See DECISIONS.md D001: "10 Hz fixed simulation tick."
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

private:
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_cellCount;
    uint64_t m_tick = 0;

    /* Per-cell SoA channels.
     * Fixed-size after construction, logically immutable until
     * future tasks add commands or simulation systems. */
    std::vector<uint8_t> m_terrain;
    std::vector<uint8_t> m_occupancy;
};

} // namespace axiom

#endif // AXIOM_CORE_WORLD_H