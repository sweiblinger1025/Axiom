/*
 * ax_math_div.h — Division variants for Axiom safe arithmetic
 *
 * Provides three division modes per TASK-004.md:
 *   - trunc: truncate toward zero (C++ default)
 *   - floor: floor toward negative infinity
 *   - round: round to nearest, half away from zero
 *
 * Division by zero:
 *   - Debug: assert/trap
 *   - Release: saturate based on sign of dividend
 *
 * All functions are pure integer math — NO floating point.
 *
 * Governing docs:
 *   DECISIONS.md D005  — Division rounding policy
 *   TASK-004.md        — Implementation spec
 */

#ifndef AXIOM_MATH_DIV_H
#define AXIOM_MATH_DIV_H

#include "ax_math_overflow.h"
#include <cstdint>

namespace axiom {
namespace math {

/* ═══════════════════════════════════════════════════════════════════
 * TRUNCATE TOWARD ZERO
 *
 * This is the default C++ behavior for integer division.
 * Provided explicitly for clarity and consistency.
 *
 * Examples:
 *    7 /  3 =  2
 *   -7 /  3 = -2
 *    7 / -3 = -2
 *   -7 / -3 =  2
 * ═══════════════════════════════════════════════════════════════════ */

inline int32_t ax_div_trunc_i32(int32_t a, int32_t b) noexcept
{
    if (b == 0) {
        AX_DIV_ZERO_TRAP();
        return saturate_div_zero_i32(a);
    }
    return a / b;
}

inline int64_t ax_div_trunc_i64(int64_t a, int64_t b) noexcept
{
    if (b == 0) {
        AX_DIV_ZERO_TRAP();
        return saturate_div_zero_i64(a);
    }
    return a / b;
}

/* ═══════════════════════════════════════════════════════════════════
 * FLOOR TOWARD NEGATIVE INFINITY
 *
 * Always rounds down, regardless of sign.
 * Differs from truncate only when there's a remainder AND
 * the result would be negative.
 *
 * Examples:
 *    7 /  3 =  2  (same as trunc)
 *   -7 /  3 = -3  (trunc gives -2)
 *    7 / -3 = -3  (trunc gives -2)
 *   -7 / -3 =  2  (same as trunc)
 *
 * Implementation: if signs differ and there's a remainder,
 * subtract 1 from the truncated result.
 * ═══════════════════════════════════════════════════════════════════ */

inline int32_t ax_div_floor_i32(int32_t a, int32_t b) noexcept
{
    if (b == 0) {
        AX_DIV_ZERO_TRAP();
        return saturate_div_zero_i32(a);
    }

    int32_t q = a / b;
    int32_t r = a % b;

    // If remainder is non-zero and signs of a and b differ,
    // the truncated result is too high — subtract 1
    if (r != 0 && ((a ^ b) < 0)) {
        q -= 1;
    }

    return q;
}

inline int64_t ax_div_floor_i64(int64_t a, int64_t b) noexcept
{
    if (b == 0) {
        AX_DIV_ZERO_TRAP();
        return saturate_div_zero_i64(a);
    }

    int64_t q = a / b;
    int64_t r = a % b;

    if (r != 0 && ((a ^ b) < 0)) {
        q -= 1;
    }

    return q;
}

/* ═══════════════════════════════════════════════════════════════════
 * ROUND TO NEAREST, HALF AWAY FROM ZERO
 *
 * Rounds to the nearest integer. When exactly halfway (remainder
 * is exactly half the divisor), rounds away from zero.
 *
 * Examples:
 *    7 /  3 =  2  (2.33... → 2)
 *    8 /  3 =  3  (2.66... → 3)
 *    5 /  2 =  3  (2.5 → 3, half away from zero)
 *   -7 /  3 = -2  (-2.33... → -2)
 *   -8 /  3 = -3  (-2.66... → -3)
 *   -5 /  2 = -3  (-2.5 → -3, half away from zero)
 *
 * Implementation: compare 2*|remainder| with |divisor| to determine
 * if we should round away from zero.
 * ═══════════════════════════════════════════════════════════════════ */

inline int32_t ax_div_round_i32(int32_t a, int32_t b) noexcept
{
    if (b == 0) {
        AX_DIV_ZERO_TRAP();
        return saturate_div_zero_i32(a);
    }

    int32_t q = a / b;
    int32_t r = a % b;

    if (r == 0) {
        return q;
    }

    // Get absolute values for comparison
    // Use int64_t to avoid overflow when computing 2*|r|
    int64_t abs_r = (r < 0) ? -static_cast<int64_t>(r) : static_cast<int64_t>(r);
    int64_t abs_b = (b < 0) ? -static_cast<int64_t>(b) : static_cast<int64_t>(b);

    // Compare 2*|remainder| with |divisor|
    // If 2*|r| >= |b|, round away from zero
    if (2 * abs_r >= abs_b) {
        // Round away from zero: add 1 if positive result, subtract 1 if negative
        if ((a ^ b) >= 0) {
            // Same signs → positive result → round up
            q += 1;
        } else {
            // Different signs → negative result → round down (more negative)
            q -= 1;
        }
    }

    return q;
}

inline int64_t ax_div_round_i64(int64_t a, int64_t b) noexcept
{
    if (b == 0) {
        AX_DIV_ZERO_TRAP();
        return saturate_div_zero_i64(a);
    }

    int64_t q = a / b;
    int64_t r = a % b;

    if (r == 0) {
        return q;
    }

    // Get absolute values using unsigned to handle INT64_MIN safely
    uint64_t abs_r = (r < 0) ? static_cast<uint64_t>(-r) : static_cast<uint64_t>(r);
    uint64_t abs_b = (b < 0) ? static_cast<uint64_t>(-b) : static_cast<uint64_t>(b);

    // Compare 2*|remainder| with |divisor|
    // Use careful arithmetic to avoid overflow: check if abs_r >= (abs_b + 1) / 2
    // This is equivalent to 2*abs_r >= abs_b but avoids the 2* overflow risk
    uint64_t half_b = abs_b / 2;
    bool half_away = (abs_r > half_b) || (abs_r == half_b && (abs_b % 2 == 0));

    if (half_away) {
        if ((a ^ b) >= 0) {
            q += 1;
        } else {
            q -= 1;
        }
    }

    return q;
}

} // namespace math
} // namespace axiom

#endif // AXIOM_MATH_DIV_H