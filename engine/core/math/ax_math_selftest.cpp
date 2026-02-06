/*
 * ax_math_selftest.cpp — Determinism selftest implementation
 *
 * Runs a comprehensive battery of arithmetic operations and computes
 * a checksum. The checksum is deterministic: same seed always produces
 * same result, enabling cross-language verification.
 *
 * Algorithm:
 *   1. Initialize checksum state from seed
 *   2. Generate deterministic test values via xorshift32
 *   3. Run operations, fold each result into checksum
 *   4. Include edge cases (always) and overflow cases (release only)
 *   5. Return final mixed checksum
 *
 * NO FLOATING POINT anywhere in this file.
 *
 * Governing docs:
 *   TASK-004.md  — Selftest specification
 */

#include "ax_math_selftest.h"
#include "ax_math_types.h"
#include "ax_math_overflow.h"
#include "ax_math_safe.h"
#include "ax_math_div.h"

#include <cstdint>

namespace axiom {
namespace math {

/* ═══════════════════════════════════════════════════════════════════
 * MIXING PRIMITIVES
 *
 * Used to combine operation results into the checksum.
 * Based on well-known hash mixing techniques.
 * ═══════════════════════════════════════════════════════════════════ */

/// Rotate left for 64-bit values
static constexpr uint64_t rotl64(uint64_t x, int r) noexcept
{
    return (x << r) | (x >> (64 - r));
}

/// Mix a value into the hash state
static constexpr uint64_t mix(uint64_t h, uint64_t v) noexcept
{
    // Golden ratio constant for good bit distribution
    h ^= v + 0x9E3779B97F4A7C15ULL;
    h = rotl64(h, 27);
    h *= 0xC2B2AE3D27D4EB4FULL;
    h ^= (h >> 33);
    return h;
}

/* ═══════════════════════════════════════════════════════════════════
 * DETERMINISTIC VALUE GENERATOR
 *
 * Xorshift32 PRNG — simple, fast, deterministic.
 * State is maintained across calls within a single selftest run.
 * ═══════════════════════════════════════════════════════════════════ */

class Xorshift32
{
public:
    explicit Xorshift32(uint32_t seed) noexcept
        : m_state(seed ? seed : 0xA5A5A5A5u)
    {
    }

    uint32_t next() noexcept
    {
        uint32_t x = m_state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        m_state = x;
        return x;
    }

    /// Generate int32 value
    int32_t next_i32() noexcept
    {
        return static_cast<int32_t>(next());
    }

    /// Generate int64 value (combine two 32-bit values)
    int64_t next_i64() noexcept
    {
        uint64_t hi = next();
        uint64_t lo = next();
        return static_cast<int64_t>((hi << 32) | lo);
    }

    /// Generate non-zero int32 (for divisors)
    int32_t next_nonzero_i32() noexcept
    {
        int32_t v = next_i32();
        return (v == 0) ? 1 : v;
    }

    /// Generate non-zero int64 (for divisors)
    int64_t next_nonzero_i64() noexcept
    {
        int64_t v = next_i64();
        return (v == 0) ? 1 : v;
    }

private:
    uint32_t m_state;
};

/* ═══════════════════════════════════════════════════════════════════
 * CHECKSUM HELPERS
 *
 * Fold values of various types into the running checksum.
 * ═══════════════════════════════════════════════════════════════════ */

static uint64_t fold_i32(uint64_t h, int32_t v) noexcept
{
    return mix(h, static_cast<uint64_t>(static_cast<uint32_t>(v)));
}

static uint64_t fold_i64(uint64_t h, int64_t v) noexcept
{
    return mix(h, static_cast<uint64_t>(v));
}

/* ═══════════════════════════════════════════════════════════════════
 * SELFTEST IMPLEMENTATION
 * ═══════════════════════════════════════════════════════════════════ */

uint64_t ax_math_selftest_checksum(uint32_t seed) noexcept
{
    // Initialize checksum with seed mixed with version
    uint64_t h = mix(0, seed);
    h = mix(h, kMathVersion);

    // Initialize RNG
    Xorshift32 rng(seed);

    /* ─────────────────────────────────────────────────────────────────
     * SECTION 1: Temperature (int32) operations
     * ───────────────────────────────────────────────────────────────── */
    for (int i = 0; i < 50; ++i)
    {
        // Constrain to ±10,000,000 so multiply by 100 stays under INT32_MAX
        ax_temp_mK a = static_cast<int32_t>(rng.next() % 20000001u) - 10000000;
        ax_temp_mK b = static_cast<int32_t>(rng.next() % 20000001u) - 10000000;
        // Scalar 1-100: 10M * 100 = 1B, safely under 2.1B
        int32_t scalar = static_cast<int32_t>(rng.next() % 100u) + 1;
        int32_t divisor = rng.next_nonzero_i32();

        h = fold_i32(h, ax_temp_add(a, b));
        h = fold_i32(h, ax_temp_sub(a, b));
        h = fold_i32(h, ax_temp_mul(a, scalar));
        h = fold_i32(h, ax_temp_div(a, divisor));
        h = fold_i32(h, ax_temp_clamp(a, -1000000, 1000000));
    }

    /* ─────────────────────────────────────────────────────────────────
     * SECTION 2: Mass (int64) operations
     * ───────────────────────────────────────────────────────────────── */
    for (int i = 0; i < 50; ++i)
    {
        // Constrain to ±1 trillion so multiply by 1000 stays safe
        ax_mass_mg a = (static_cast<int64_t>(rng.next()) % 2000000000001LL) - 1000000000000LL;
        ax_mass_mg b = (static_cast<int64_t>(rng.next()) % 2000000000001LL) - 1000000000000LL;
        // Scalar 1-1000: 1T * 1000 = 1 quadrillion, under INT64_MAX
        int64_t scalar = static_cast<int64_t>(rng.next() % 1000u) + 1;
        int64_t divisor = rng.next_nonzero_i64();

        h = fold_i64(h, ax_mass_add(a, b));
        h = fold_i64(h, ax_mass_sub(a, b));
        h = fold_i64(h, ax_mass_mul(a, scalar));
        h = fold_i64(h, ax_mass_div(a, divisor));
        h = fold_i64(h, ax_mass_clamp(a, -1000000000LL, 1000000000LL));
    }

    /* ─────────────────────────────────────────────────────────────────
     * SECTION 3: Energy (int64) operations
     * ───────────────────────────────────────────────────────────────── */
    for (int i = 0; i < 50; ++i)
    {
        // Constrain to ±1 trillion so multiply by 1000 stays safe
        ax_energy_mJ a = (static_cast<int64_t>(rng.next()) % 2000000000001LL) - 1000000000000LL;
        ax_energy_mJ b = (static_cast<int64_t>(rng.next()) % 2000000000001LL) - 1000000000000LL;
        // Scalar 1-1000: 1T * 1000 = 1 quadrillion, under INT64_MAX
        int64_t scalar = static_cast<int64_t>(rng.next() % 1000u) + 1;
        int64_t divisor = rng.next_nonzero_i64();

        h = fold_i64(h, ax_energy_add(a, b));
        h = fold_i64(h, ax_energy_sub(a, b));
        h = fold_i64(h, ax_energy_mul(a, scalar));
        h = fold_i64(h, ax_energy_div(a, divisor));
        h = fold_i64(h, ax_energy_clamp(a, -1000000000LL, 1000000000LL));
    }

    /* ─────────────────────────────────────────────────────────────────
     * SECTION 4: Division variants (trunc/floor/round)
     * ───────────────────────────────────────────────────────────────── */
    for (int i = 0; i < 30; ++i)
    {
        int32_t a32 = rng.next_i32();
        int32_t b32 = rng.next_nonzero_i32();
        int64_t a64 = rng.next_i64();
        int64_t b64 = rng.next_nonzero_i64();

        // int32 variants
        h = fold_i32(h, ax_div_trunc_i32(a32, b32));
        h = fold_i32(h, ax_div_floor_i32(a32, b32));
        h = fold_i32(h, ax_div_round_i32(a32, b32));

        // int64 variants
        h = fold_i64(h, ax_div_trunc_i64(a64, b64));
        h = fold_i64(h, ax_div_floor_i64(a64, b64));
        h = fold_i64(h, ax_div_round_i64(a64, b64));
    }

    /* ─────────────────────────────────────────────────────────────────
     * SECTION 5: Known edge cases (always included)
     *
     * These are deterministic cases that don't cause overflow,
     * verifying correct behavior at boundaries.
     * ───────────────────────────────────────────────────────────────── */

    // Zero operations
    h = fold_i32(h, ax_temp_add(0, 0));
    h = fold_i32(h, ax_temp_sub(0, 0));
    h = fold_i32(h, ax_temp_mul(0, 12345));
    h = fold_i32(h, ax_temp_mul(12345, 0));
    h = fold_i32(h, ax_temp_div(0, 1));

    h = fold_i64(h, ax_mass_add(0, 0));
    h = fold_i64(h, ax_mass_sub(0, 0));
    h = fold_i64(h, ax_mass_mul(0, 12345LL));
    h = fold_i64(h, ax_mass_mul(12345LL, 0));
    h = fold_i64(h, ax_mass_div(0, 1));

    // Division edge cases (from TASK-004 acceptance criteria)
    // trunc: 7/3=2, -7/3=-2
    h = fold_i32(h, ax_div_trunc_i32(7, 3));    // expect 2
    h = fold_i32(h, ax_div_trunc_i32(-7, 3));   // expect -2

    // floor: 7/3=2, -7/3=-3
    h = fold_i32(h, ax_div_floor_i32(7, 3));    // expect 2
    h = fold_i32(h, ax_div_floor_i32(-7, 3));   // expect -3

    // round: -7/3=-2, 8/3=3
    h = fold_i32(h, ax_div_round_i32(-7, 3));   // expect -2
    h = fold_i32(h, ax_div_round_i32(8, 3));    // expect 3

    // Clamp edge cases
    h = fold_i32(h, ax_temp_clamp(500, 0, 1000));     // in range
    h = fold_i32(h, ax_temp_clamp(-500, 0, 1000));    // below → 0
    h = fold_i32(h, ax_temp_clamp(1500, 0, 1000));    // above → 1000

    // Near-boundary operations that don't overflow
    h = fold_i32(h, ax_temp_add(INT32_MAX - 1, 1));   // just under max
    h = fold_i32(h, ax_temp_sub(INT32_MIN + 1, 1));   // just above min
    h = fold_i64(h, ax_mass_add(INT64_MAX - 1, 1));
    h = fold_i64(h, ax_mass_sub(INT64_MIN + 1, 1));

    /* ─────────────────────────────────────────────────────────────────
     * SECTION 6: Overflow/saturation cases (RELEASE MODE ONLY)
     *
     * In debug mode, these would trap. We skip them.
     * In release mode, these verify saturation behavior.
     * ───────────────────────────────────────────────────────────────── */

    #ifndef AX_MATH_OVERFLOW_ASSERT
    {
        // Addition overflow → saturation
        h = fold_i32(h, ax_temp_add(INT32_MAX, 1));        // → INT32_MAX
        h = fold_i32(h, ax_temp_add(INT32_MIN, -1));       // → INT32_MIN
        h = fold_i64(h, ax_mass_add(INT64_MAX, 1));
        h = fold_i64(h, ax_mass_add(INT64_MIN, -1));

        // Subtraction overflow → saturation
        h = fold_i32(h, ax_temp_sub(INT32_MIN, 1));        // → INT32_MIN
        h = fold_i32(h, ax_temp_sub(INT32_MAX, -1));       // → INT32_MAX
        h = fold_i64(h, ax_mass_sub(INT64_MIN, 1));
        h = fold_i64(h, ax_mass_sub(INT64_MAX, -1));

        // Multiplication overflow → saturation
        h = fold_i32(h, ax_temp_mul(INT32_MAX, 2));        // → INT32_MAX
        h = fold_i32(h, ax_temp_mul(INT32_MIN, 2));        // → INT32_MIN
        h = fold_i64(h, ax_mass_mul(INT64_MAX, 2));
        h = fold_i64(h, ax_mass_mul(INT64_MIN, 2));

        // Division by zero → saturation based on sign
        h = fold_i32(h, ax_div_trunc_i32(100, 0));         // → INT32_MAX
        h = fold_i32(h, ax_div_trunc_i32(-100, 0));        // → INT32_MIN
        h = fold_i64(h, ax_div_trunc_i64(100, 0));
        h = fold_i64(h, ax_div_trunc_i64(-100, 0));

        // Division floor/round with zero divisor
        h = fold_i32(h, ax_div_floor_i32(50, 0));
        h = fold_i32(h, ax_div_round_i32(-50, 0));
        h = fold_i64(h, ax_div_floor_i64(50, 0));
        h = fold_i64(h, ax_div_round_i64(-50, 0));
    }
    #endif

    /* ─────────────────────────────────────────────────────────────────
     * Final mixing pass
     * ───────────────────────────────────────────────────────────────── */
    h ^= h >> 33;
    h *= 0xFF51AFD7ED558CCDULL;
    h ^= h >> 33;
    h *= 0xC4CEB9FE1A85EC53ULL;
    h ^= h >> 33;

    return h;
}

} // namespace math
} // namespace axiom