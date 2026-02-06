/*
 * ax_math_safe.h — Safe arithmetic for Axiom canonical types
 *
 * Provides overflow-checked arithmetic operations for the three
 * canonical quantity types defined in ax_math_types.h:
 *   - ax_temp_mK   (temperature in milliKelvin)
 *   - ax_mass_mg   (mass in milligrams)
 *   - ax_energy_mJ (energy in milliJoules)
 *
 * Overflow behavior:
 *   - Debug:   assert/trap (catch bugs immediately)
 *   - Release: saturate to type bounds (prevent corruption)
 *
 * All functions are pure integer math — NO floating point.
 *
 * Usage:
 *   ax_temp_mK t1 = 300000;  // 300 K
 *   ax_temp_mK t2 = 50000;   // 50 K
 *   ax_temp_mK sum = ax_temp_add(t1, t2);  // 350 K, overflow-safe
 *
 * Governing docs:
 *   DECISIONS.md D005  — Canonical fixed-point policy
 *   TASK-004.md        — Implementation spec
 */

#ifndef AXIOM_MATH_SAFE_H
#define AXIOM_MATH_SAFE_H

#include "ax_math_types.h"
#include <cstdint>

namespace axiom {
namespace math {

/* ═══════════════════════════════════════════════════════════════════
 * TEMPERATURE (ax_temp_mK) — int32, milliKelvin
 *
 * Safe arithmetic for temperature values.
 * Scalar operations use int32_t scalars.
 * ═══════════════════════════════════════════════════════════════════ */

/// Add two temperatures. Overflow-safe.
ax_temp_mK ax_temp_add(ax_temp_mK a, ax_temp_mK b) noexcept;

/// Subtract temperatures (a - b). Overflow-safe.
ax_temp_mK ax_temp_sub(ax_temp_mK a, ax_temp_mK b) noexcept;

/// Multiply temperature by scalar. Overflow-safe.
ax_temp_mK ax_temp_mul(ax_temp_mK a, int32_t scalar) noexcept;

/// Divide temperature by scalar. Division by zero handled per policy.
ax_temp_mK ax_temp_div(ax_temp_mK a, int32_t divisor) noexcept;

/// Clamp temperature to [lo, hi] range.
ax_temp_mK ax_temp_clamp(ax_temp_mK val, ax_temp_mK lo, ax_temp_mK hi) noexcept;

/* ═══════════════════════════════════════════════════════════════════
 * MASS (ax_mass_mg) — int64, milligrams
 *
 * Safe arithmetic for mass values.
 * Scalar operations use int64_t scalars.
 * ═══════════════════════════════════════════════════════════════════ */

/// Add two masses. Overflow-safe.
ax_mass_mg ax_mass_add(ax_mass_mg a, ax_mass_mg b) noexcept;

/// Subtract masses (a - b). Overflow-safe.
ax_mass_mg ax_mass_sub(ax_mass_mg a, ax_mass_mg b) noexcept;

/// Multiply mass by scalar. Overflow-safe.
ax_mass_mg ax_mass_mul(ax_mass_mg a, int64_t scalar) noexcept;

/// Divide mass by scalar. Division by zero handled per policy.
ax_mass_mg ax_mass_div(ax_mass_mg a, int64_t divisor) noexcept;

/// Clamp mass to [lo, hi] range.
ax_mass_mg ax_mass_clamp(ax_mass_mg val, ax_mass_mg lo, ax_mass_mg hi) noexcept;

/* ═══════════════════════════════════════════════════════════════════
 * ENERGY (ax_energy_mJ) — int64, milliJoules
 *
 * Safe arithmetic for energy values.
 * Scalar operations use int64_t scalars.
 * ═══════════════════════════════════════════════════════════════════ */

/// Add two energies. Overflow-safe.
ax_energy_mJ ax_energy_add(ax_energy_mJ a, ax_energy_mJ b) noexcept;

/// Subtract energies (a - b). Overflow-safe.
ax_energy_mJ ax_energy_sub(ax_energy_mJ a, ax_energy_mJ b) noexcept;

/// Multiply energy by scalar. Overflow-safe.
ax_energy_mJ ax_energy_mul(ax_energy_mJ a, int64_t scalar) noexcept;

/// Divide energy by scalar. Division by zero handled per policy.
ax_energy_mJ ax_energy_div(ax_energy_mJ a, int64_t divisor) noexcept;

/// Clamp energy to [lo, hi] range.
ax_energy_mJ ax_energy_clamp(ax_energy_mJ val, ax_energy_mJ lo, ax_energy_mJ hi) noexcept;

} // namespace math
} // namespace axiom

#endif // AXIOM_MATH_SAFE_H