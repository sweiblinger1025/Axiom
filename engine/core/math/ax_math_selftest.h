/*
* ax_math_selftest.h — Determinism selftest for Axiom math utilities
 *
 * Provides a checksum function that exercises all safe arithmetic
 * operations. Identical checksums from C++ and C# prove cross-language
 * determinism.
 *
 * Usage:
 *   uint64_t checksum = ax_math_selftest_checksum(0);
 *   // Compare with C# result — must match exactly
 *
 * Governing docs:
 *   TASK-004.md  — Selftest specification
 */

#ifndef AXIOM_MATH_SELFTEST_H
#define AXIOM_MATH_SELFTEST_H

#include <cstdint>

namespace axiom {
    namespace math {

        /// Math utilities version. Bumped on any change to arithmetic behavior.
        /// Note: Named differently from C API macro to avoid preprocessor conflicts.
        constexpr uint32_t kMathVersion = 1u;

        /*
         * Run a deterministic battery of arithmetic operations and return
         * a checksum computed from the results.
         *
         * Contract:
         *   - Same seed → same checksum, always
         *   - Same checksum from C++ and C# proves determinism
         *   - Checksum algorithm is stable within AX_MATH_VERSION
         *
         * Parameters:
         *   seed: Controls test value generation.
         *         seed=0 runs the canonical test battery.
         *         Other seeds provide additional coverage.
         *
         * Returns:
         *   64-bit checksum of all operation results.
         *
         * Note on overflow testing:
         *   In debug builds (AX_MATH_OVERFLOW_ASSERT), overflow cases are
         *   skipped to avoid trapping. In release builds, overflow cases
         *   are included and verify saturation behavior.
         */
        uint64_t ax_math_selftest_checksum(uint32_t seed) noexcept;

    } // namespace math
} // namespace axiom

#endif // AXIOM_MATH_SELFTEST_H