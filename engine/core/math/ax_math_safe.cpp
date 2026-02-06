/*
 * ax_math_safe.cpp — Safe arithmetic implementations
 *
 * Implements overflow-checked arithmetic for canonical types.
 * Uses detection helpers from ax_math_overflow.h and division
 * from ax_math_div.h.
 *
 * Governing docs:
 *   DECISIONS.md D005  — Canonical fixed-point policy
 *   TASK-004.md        — Implementation spec
 */

#include "ax_math_safe.h"
#include "ax_math_overflow.h"
#include "ax_math_div.h"

namespace axiom {
namespace math {

/* ═══════════════════════════════════════════════════════════════════
 * TEMPERATURE (ax_temp_mK) — int32
 * ═══════════════════════════════════════════════════════════════════ */

ax_temp_mK ax_temp_add(ax_temp_mK a, ax_temp_mK b) noexcept
{
    if (would_overflow_add_i32(a, b)) {
        AX_OVERFLOW_TRAP();
        return saturate_add_i32(a, b);
    }
    return a + b;
}

ax_temp_mK ax_temp_sub(ax_temp_mK a, ax_temp_mK b) noexcept
{
    if (would_overflow_sub_i32(a, b)) {
        AX_OVERFLOW_TRAP();
        return saturate_sub_i32(a, b);
    }
    return a - b;
}

ax_temp_mK ax_temp_mul(ax_temp_mK a, int32_t scalar) noexcept
{
    if (would_overflow_mul_i32(a, scalar)) {
        AX_OVERFLOW_TRAP();
        return saturate_mul_i32(a, scalar);
    }
    return a * scalar;
}

ax_temp_mK ax_temp_div(ax_temp_mK a, int32_t divisor) noexcept
{
    // Division uses truncate-toward-zero by default
    // ax_div_trunc_i32 handles div-by-zero per policy
    return ax_div_trunc_i32(a, divisor);
}

ax_temp_mK ax_temp_clamp(ax_temp_mK val, ax_temp_mK lo, ax_temp_mK hi) noexcept
{
    return clamp(val, lo, hi);
}

/* ═══════════════════════════════════════════════════════════════════
 * MASS (ax_mass_mg) — int64
 * ═══════════════════════════════════════════════════════════════════ */

ax_mass_mg ax_mass_add(ax_mass_mg a, ax_mass_mg b) noexcept
{
    if (would_overflow_add_i64(a, b)) {
        AX_OVERFLOW_TRAP();
        return saturate_add_i64(a, b);
    }
    return a + b;
}

ax_mass_mg ax_mass_sub(ax_mass_mg a, ax_mass_mg b) noexcept
{
    if (would_overflow_sub_i64(a, b)) {
        AX_OVERFLOW_TRAP();
        return saturate_sub_i64(a, b);
    }
    return a - b;
}

ax_mass_mg ax_mass_mul(ax_mass_mg a, int64_t scalar) noexcept
{
    if (would_overflow_mul_i64(a, scalar)) {
        AX_OVERFLOW_TRAP();
        return saturate_mul_i64(a, scalar);
    }
    return a * scalar;
}

ax_mass_mg ax_mass_div(ax_mass_mg a, int64_t divisor) noexcept
{
    return ax_div_trunc_i64(a, divisor);
}

ax_mass_mg ax_mass_clamp(ax_mass_mg val, ax_mass_mg lo, ax_mass_mg hi) noexcept
{
    return clamp(val, lo, hi);
}

/* ═══════════════════════════════════════════════════════════════════
 * ENERGY (ax_energy_mJ) — int64
 * ═══════════════════════════════════════════════════════════════════ */

ax_energy_mJ ax_energy_add(ax_energy_mJ a, ax_energy_mJ b) noexcept
{
    if (would_overflow_add_i64(a, b)) {
        AX_OVERFLOW_TRAP();
        return saturate_add_i64(a, b);
    }
    return a + b;
}

ax_energy_mJ ax_energy_sub(ax_energy_mJ a, ax_energy_mJ b) noexcept
{
    if (would_overflow_sub_i64(a, b)) {
        AX_OVERFLOW_TRAP();
        return saturate_sub_i64(a, b);
    }
    return a - b;
}

ax_energy_mJ ax_energy_mul(ax_energy_mJ a, int64_t scalar) noexcept
{
    if (would_overflow_mul_i64(a, scalar)) {
        AX_OVERFLOW_TRAP();
        return saturate_mul_i64(a, scalar);
    }
    return a * scalar;
}

ax_energy_mJ ax_energy_div(ax_energy_mJ a, int64_t divisor) noexcept
{
    return ax_div_trunc_i64(a, divisor);
}

ax_energy_mJ ax_energy_clamp(ax_energy_mJ val, ax_energy_mJ lo, ax_energy_mJ hi) noexcept
{
    return clamp(val, lo, hi);
}

} // namespace math
} // namespace axiom