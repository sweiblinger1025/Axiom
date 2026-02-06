/*
 * ax_math_overflow.h — Overflow policy for Axiom safe arithmetic
 *
 * Defines the compile-time overflow policy and detection helpers.
 *
 * Policy (from TASK-004.md):
 *   Debug:   assert/trap on overflow (catch bugs immediately)
 *   Release: saturate to bounds (prevent corruption cascades)
 *
 * The policy is GLOBAL — no per-subsystem variation.
 *
 * Usage:
 *   if (AX_WOULD_OVERFLOW_ADD_I32(a, b)) {
 *       AX_OVERFLOW_TRAP();           // debug: stops here
 *       return AX_SATURATE_I32(a, b); // release: returns clamped value
 *   }
 *
 * Governing docs:
 *   DECISIONS.md D005  — Overflow policy
 *   TASK-004.md        — Implementation spec
 */

#ifndef AXIOM_MATH_OVERFLOW_H
#define AXIOM_MATH_OVERFLOW_H

#include <cstdint>
#include <climits>
#include <cassert>

namespace axiom {
namespace math {

/* ─────────────────────────────────────────────────────────────────
 * Overflow Policy Configuration
 *
 * AX_MATH_OVERFLOW_ASSERT: when defined, overflow triggers assert().
 * When not defined, overflow saturates to type bounds.
 *
 * Typically set by CMake:
 *   - Debug builds:   -DAX_MATH_OVERFLOW_ASSERT
 *   - Release builds: (not defined)
 * ───────────────────────────────────────────────────────────────── */

#ifdef AX_MATH_OVERFLOW_ASSERT
    #define AX_OVERFLOW_TRAP() assert(false && "arithmetic overflow")
    #define AX_DIV_ZERO_TRAP() assert(false && "division by zero")
    constexpr bool AX_OVERFLOW_TRAPS = true;
#else
    #define AX_OVERFLOW_TRAP() ((void)0)
    #define AX_DIV_ZERO_TRAP() ((void)0)
    constexpr bool AX_OVERFLOW_TRAPS = false;
#endif

/* ─────────────────────────────────────────────────────────────────
 * Overflow Detection: 32-bit signed addition
 *
 * Returns true if a + b would overflow int32_t.
 * Uses the standard overflow detection pattern:
 *   - If b > 0 and a > MAX - b → overflow (positive)
 *   - If b < 0 and a < MIN - b → overflow (negative)
 * ───────────────────────────────────────────────────────────────── */

constexpr bool would_overflow_add_i32(int32_t a, int32_t b) noexcept
{
    if (b > 0 && a > INT32_MAX - b) return true;
    if (b < 0 && a < INT32_MIN - b) return true;
    return false;
}

/* ─────────────────────────────────────────────────────────────────
 * Overflow Detection: 32-bit signed subtraction
 *
 * Returns true if a - b would overflow int32_t.
 * Subtraction overflow:
 *   - If b < 0 and a > MAX + b → overflow (positive)
 *   - If b > 0 and a < MIN + b → overflow (negative)
 * ───────────────────────────────────────────────────────────────── */

constexpr bool would_overflow_sub_i32(int32_t a, int32_t b) noexcept
{
    if (b < 0 && a > INT32_MAX + b) return true;
    if (b > 0 && a < INT32_MIN + b) return true;
    return false;
}

/* ─────────────────────────────────────────────────────────────────
 * Overflow Detection: 64-bit signed addition
 * ───────────────────────────────────────────────────────────────── */

constexpr bool would_overflow_add_i64(int64_t a, int64_t b) noexcept
{
    if (b > 0 && a > INT64_MAX - b) return true;
    if (b < 0 && a < INT64_MIN - b) return true;
    return false;
}

/* ─────────────────────────────────────────────────────────────────
 * Overflow Detection: 64-bit signed subtraction
 * ───────────────────────────────────────────────────────────────── */

constexpr bool would_overflow_sub_i64(int64_t a, int64_t b) noexcept
{
    if (b < 0 && a > INT64_MAX + b) return true;
    if (b > 0 && a < INT64_MIN + b) return true;
    return false;
}

/* ─────────────────────────────────────────────────────────────────
 * Overflow Detection: 32-bit signed multiplication
 *
 * For 32×32, we widen to 64-bit and check if result fits in 32.
 * This is the "always widen before multiply" strategy from TASK-004.
 * ───────────────────────────────────────────────────────────────── */

constexpr bool would_overflow_mul_i32(int32_t a, int32_t b) noexcept
{
    int64_t wide = static_cast<int64_t>(a) * static_cast<int64_t>(b);
    return wide < INT32_MIN || wide > INT32_MAX;
}

/* ─────────────────────────────────────────────────────────────────
 * Overflow Detection: 64-bit signed multiplication
 *
 * For 64×64, we need 128-bit intermediate. This is platform-specific.
 * On platforms without __int128, we use a conservative check.
 * ───────────────────────────────────────────────────────────────── */

#if defined(__SIZEOF_INT128__)
    // GCC/Clang with __int128 support
    constexpr bool would_overflow_mul_i64(int64_t a, int64_t b) noexcept
    {
        __int128 wide = static_cast<__int128>(a) * static_cast<__int128>(b);
        return wide < INT64_MIN || wide > INT64_MAX;
    }
#else
    // MSVC or platforms without __int128: conservative check
    // This may report false positives but never false negatives
    inline bool would_overflow_mul_i64(int64_t a, int64_t b) noexcept
    {
        if (a == 0 || b == 0) return false;
        if (a == -1) return b == INT64_MIN;
        if (b == -1) return a == INT64_MIN;

        // Check if |a| * |b| would exceed INT64_MAX
        // Using unsigned to avoid signed overflow during the check
        uint64_t abs_a = (a < 0) ? static_cast<uint64_t>(-a) : static_cast<uint64_t>(a);
        uint64_t abs_b = (b < 0) ? static_cast<uint64_t>(-b) : static_cast<uint64_t>(b);

        // If abs_a > MAX / abs_b, overflow would occur
        if (abs_a > static_cast<uint64_t>(INT64_MAX) / abs_b) return true;

        // Check sign: if signs differ, result is negative
        // Negative result can be as low as INT64_MIN
        bool result_negative = (a < 0) != (b < 0);
        uint64_t product = abs_a * abs_b;

        if (result_negative) {
            // Can represent down to -INT64_MAX-1 = INT64_MIN
            return product > static_cast<uint64_t>(INT64_MAX) + 1;
        } else {
            return product > static_cast<uint64_t>(INT64_MAX);
        }
    }
#endif

/* ─────────────────────────────────────────────────────────────────
 * Saturated Results
 *
 * When overflow is detected in release mode, these compute the
 * saturated (clamped) result.
 * ───────────────────────────────────────────────────────────────── */

/// Saturated add for int32: clamps to INT32_MIN/MAX on overflow.
constexpr int32_t saturate_add_i32(int32_t a, int32_t b) noexcept
{
    if (b > 0 && a > INT32_MAX - b) return INT32_MAX;
    if (b < 0 && a < INT32_MIN - b) return INT32_MIN;
    return a + b;
}

/// Saturated subtract for int32.
constexpr int32_t saturate_sub_i32(int32_t a, int32_t b) noexcept
{
    if (b < 0 && a > INT32_MAX + b) return INT32_MAX;
    if (b > 0 && a < INT32_MIN + b) return INT32_MIN;
    return a - b;
}

/// Saturated add for int64.
constexpr int64_t saturate_add_i64(int64_t a, int64_t b) noexcept
{
    if (b > 0 && a > INT64_MAX - b) return INT64_MAX;
    if (b < 0 && a < INT64_MIN - b) return INT64_MIN;
    return a + b;
}

/// Saturated subtract for int64.
constexpr int64_t saturate_sub_i64(int64_t a, int64_t b) noexcept
{
    if (b < 0 && a > INT64_MAX + b) return INT64_MAX;
    if (b > 0 && a < INT64_MIN + b) return INT64_MIN;
    return a - b;
}

/// Saturated multiply for int32 (widens to 64-bit internally).
constexpr int32_t saturate_mul_i32(int32_t a, int32_t b) noexcept
{
    int64_t wide = static_cast<int64_t>(a) * static_cast<int64_t>(b);
    if (wide > INT32_MAX) return INT32_MAX;
    if (wide < INT32_MIN) return INT32_MIN;
    return static_cast<int32_t>(wide);
}

/// Saturated multiply for int64 (uses 128-bit where available).
#if defined(__SIZEOF_INT128__)
constexpr int64_t saturate_mul_i64(int64_t a, int64_t b) noexcept
{
    __int128 wide = static_cast<__int128>(a) * static_cast<__int128>(b);
    if (wide > INT64_MAX) return INT64_MAX;
    if (wide < INT64_MIN) return INT64_MIN;
    return static_cast<int64_t>(wide);
}
#else
inline int64_t saturate_mul_i64(int64_t a, int64_t b) noexcept
{
    if (would_overflow_mul_i64(a, b)) {
        // Determine sign of result for saturation direction
        bool result_negative = (a < 0) != (b < 0);
        return result_negative ? INT64_MIN : INT64_MAX;
    }
    return a * b;
}
#endif

/* ─────────────────────────────────────────────────────────────────
 * Division by Zero Saturation
 *
 * Per TASK-004: in release mode, a/0 saturates based on sign of a.
 *   a >= 0 → INT_MAX
 *   a <  0 → INT_MIN
 * ───────────────────────────────────────────────────────────────── */

constexpr int32_t saturate_div_zero_i32(int32_t a) noexcept
{
    return (a >= 0) ? INT32_MAX : INT32_MIN;
}

constexpr int64_t saturate_div_zero_i64(int64_t a) noexcept
{
    return (a >= 0) ? INT64_MAX : INT64_MIN;
}

/* ─────────────────────────────────────────────────────────────────
 * Generic Clamp (no overflow possible, just bounds enforcement)
 * ───────────────────────────────────────────────────────────────── */

template<typename T>
constexpr T clamp(T val, T lo, T hi) noexcept
{
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

} // namespace math
} // namespace axiom

#endif // AXIOM_MATH_OVERFLOW_H