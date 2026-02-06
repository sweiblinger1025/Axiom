/*
 * ax_math_types.h — Canonical numeric types for Axiom simulation
 *
 * Defines domain-specific scaled integer types per DECISIONS.md D005.
 * These types are self-documenting: the type name IS the unit.
 *
 * All simulation math must use these types (or their safe arithmetic
 * wrappers) to ensure determinism and prevent unit confusion.
 *
 * Governing docs:
 *   DECISIONS.md D005  — Canonical fixed-point policy
 *   UNIT_SYSTEM.md     — SI-based canonical units
 *   TASK-004.md        — Implementation spec
 */

#ifndef AXIOM_MATH_TYPES_H
#define AXIOM_MATH_TYPES_H

#include <cstdint>
#include <climits>

namespace axiom {
namespace math {

/* ─────────────────────────────────────────────────────────────────
 * Canonical Quantity Types
 *
 * Each type represents a physical quantity at a specific scale.
 * The type name encodes the unit, eliminating "what scale is this?" bugs.
 *
 * | Type         | Storage | Scale       | Range                    |
 * |--------------|---------|-------------|--------------------------|
 * | ax_temp_mK   | int32   | milliKelvin | ±2.1 million K           |
 * | ax_mass_mg   | int64   | milligram   | ±9.2 quintillion mg      |
 * | ax_energy_mJ | int64   | milliJoule  | ±9.2 quintillion mJ      |
 *
 * Physical constraints (e.g., temperature ≥ 0 K) are enforced by
 * simulation systems, not by these types.
 * ───────────────────────────────────────────────────────────────── */

/// Temperature in milliKelvin (mK). 1000 mK = 1 K.
using ax_temp_mK = int32_t;

/// Mass in milligrams (mg). 1000 mg = 1 g.
using ax_mass_mg = int64_t;

/// Energy in milliJoules (mJ). 1000 mJ = 1 J.
using ax_energy_mJ = int64_t;

/* ─────────────────────────────────────────────────────────────────
 * Type Bounds
 *
 * Explicit min/max constants for each type. Used by safe arithmetic
 * for saturation and by simulation systems for validation.
 * ───────────────────────────────────────────────────────────────── */

constexpr ax_temp_mK   AX_TEMP_MK_MIN   = INT32_MIN;
constexpr ax_temp_mK   AX_TEMP_MK_MAX   = INT32_MAX;

constexpr ax_mass_mg   AX_MASS_MG_MIN   = INT64_MIN;
constexpr ax_mass_mg   AX_MASS_MG_MAX   = INT64_MAX;

constexpr ax_energy_mJ AX_ENERGY_MJ_MIN = INT64_MIN;
constexpr ax_energy_mJ AX_ENERGY_MJ_MAX = INT64_MAX;

/* ─────────────────────────────────────────────────────────────────
 * Static Size Guarantees
 *
 * These asserts fire at compile time if type sizes don't match
 * expectations. Required for ABI compatibility with C# viewer.
 * ───────────────────────────────────────────────────────────────── */

static_assert(sizeof(ax_temp_mK)   == 4, "ax_temp_mK must be 4 bytes");
static_assert(sizeof(ax_mass_mg)   == 8, "ax_mass_mg must be 8 bytes");
static_assert(sizeof(ax_energy_mJ) == 8, "ax_energy_mJ must be 8 bytes");

} // namespace math
} // namespace axiom

#endif // AXIOM_MATH_TYPES_H