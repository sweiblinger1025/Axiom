/*
 * world.h - Axiom World (simulation truth)
 *
 * The World is the authoritative runtime object that owns all
 * simulation state. For TASK-001, that state is minimal:
 *   - world dimmensions (immmutable)
 *   - tick counter
 *
 * This is internal engine code. External access occurs only
 * through the C API (ax_api.h).
 *
 * Governing docs:
 *   ARCHITECTURE_OVERVIEW.md  - "C++ Owns Truth"
 *   SPATIAL_MODEL.md          - coordinate system, bounds
 *   TRUTH_VS_PRESENTATION.md  - no presentation concerns here
 */

#ifndef AXIOM_CORE_WORLD_H
#define AXIOM_CORE_WORLD_H

#include <cstdint>

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
     *
     * Initial state:
     *   tick = 0
     *
     *  Dimensions are immutable after construction.
     *  See SPATIAL_MODEL.md: "Bounds are defined at world creation time."
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
     * For TASK-001 there are no simulation systems - this just
     * advances the counter. Future milestones will add system
     * updates within each tick.
     *
     * Precondition: count > 0 (enforced by caller).
     * See DECISIONS.md D001: "10 Hz fixed simulation tick."
     */
    void step(uint32_t count);

    /* -- State queries ------------------------------------ */

    uint64_t tick() const  { return m_tick; }
    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }

private:
    uint32_t m_width;
    uint32_t m_height;
    uint64_t m_tick = 0;
};

} // namespace axiom

#endif // AXIOM_CORE_WORLD_H
