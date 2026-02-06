# TASK-004 — Lock D005: Canonical Numeric Policy

## Goal

Lock **DECISIONS.md D005** by establishing the canonical numeric types, arithmetic
utilities, and overflow policies that all future simulation systems will depend on.

This task resolves the remaining TBD in D005 by locking:
- Domain-specific scaled integer types for temperature, mass, and energy
- Safe arithmetic utilities with explicit overflow handling
- Cross-language determinism proof via selftest checksum

This task does NOT implement any simulation system. It provides the **numeric
foundation** that thermal, gas, liquid, and all other systems will build upon.

After TASK-004:
- D005 is locked and documented
- Safe arithmetic utilities exist for the three core quantities
- Cross-language determinism is proven via selftest checksum
- Future simulation tasks can proceed without numeric ambiguity

---

## Non-Goals

- No simulation systems (thermal, gas, liquid, power)
- No physics equations or diffusion math
- No new snapshot channels
- No commands beyond what TASK-003 established
- No large C API surface for math operations
- No pressure, volume, or flow rate types (added when needed)
- No rendering or visualization

---

## Design Decisions (Locking D005)

This section locks the numeric policy. Once TASK-004 is complete, these become
architectural invariants and D005 in `DECISIONS.md` must be updated accordingly.

### Approach: Domain-Specific Scaled Integers

Per `UNIT_SYSTEM.md`, canonical quantities are stored as **scaled integers**
with explicit, self-documenting types. This is NOT a universal fixed-point
format (like Q16.16); each quantity has its own scale chosen for optimal
range and precision.

Rationale:
- Types are self-describing (the type *is* the unit)
- Scale factors are domain-appropriate
- Cross-quantity math requires explicit dimensional handling (this is a feature)
- Aligns with existing `UNIT_SYSTEM.md` examples

### Canonical Types (v0)

| Quantity    | Type Alias     | Storage  | Scale       | Range                                         |
|-------------|----------------|----------|-------------|-----------------------------------------------|
| Temperature | `ax_temp_mK`   | `int32`  | milliKelvin | ±2.1 million K representable                  |
| Mass        | `ax_mass_mg`   | `int64`  | milligram   | ±9.2 quintillion mg                           |
| Energy      | `ax_energy_mJ` | `int64`  | milliJoule  | ±9.2 quintillion mJ                           |

Notes:
- Temperature uses signed int32 for arithmetic headroom (deltas can be negative)
- Physical range constraints (e.g., temperature ≥ 0 K) are enforced by simulation
  systems, not by the type itself
- Mass and energy use int64 for conservation tracking across large worlds
- Additional types (pressure, volume, flow rate) will be added in future tasks

### Overflow Policy

| Build Mode | Behavior          | Rationale                              |
|------------|-------------------|----------------------------------------|
| Debug      | Assert/trap       | Catch scale factor bugs immediately    |
| Release    | Saturate to bounds| Prevent corruption cascades            |

Implementation:
- Controlled via compile-time flag: `AX_MATH_OVERFLOW_ASSERT` (debug) vs default saturate
- Policy is **global** — no per-subsystem variation
- Saturation clamps to `INT32_MIN/MAX` or `INT64_MIN/MAX` as appropriate

### Division Rounding

Default: **truncate toward zero** (matches C++ behavior).

Utilities provided:
- `ax_div_trunc(a, b)` — truncate toward zero (default)
- `ax_div_floor(a, b)` — floor toward negative infinity
- `ax_div_round(a, b)` — round to nearest, half away from zero

Division by zero behavior:
- Debug: assert/trap
- Release: saturate to `INT_MAX` if `a >= 0`, `INT_MIN` if `a < 0`
  (analogous for 64-bit types)

### Multiplication Strategy

**Always widen before multiply, then scale.**

- 32-bit × 32-bit → use 64-bit intermediate
- 64-bit × 64-bit → use 128-bit intermediate where available

Platform support:
- GCC/Clang: `__int128`
- MSVC: `_mul128` / `_umul128` intrinsics, or portable fallback

At 10 Hz tick rate, correctness dominates performance concerns.

### Floating-Point Prohibition

**All `ax_math_*` utilities must NOT use floating point.**

Forbidden:
- `float` / `double` types
- `std::round`, `std::floor`, `std::ceil`
- Platform math library calls (`<cmath>`, `<math.h>`)

Rationale: Floating-point behavior varies subtly across platforms and compiler
settings. Integer-only arithmetic guarantees deterministic checksums.

---

## Internal Utilities (C++ Only)

The following utilities are implemented as internal C++ code. They are NOT
exposed through the C API (except indirectly via the selftest checksum).

### Safe Arithmetic

For each canonical type, provide:

```cpp
// Temperature (int32)
ax_temp_mK ax_temp_add(ax_temp_mK a, ax_temp_mK b);
ax_temp_mK ax_temp_sub(ax_temp_mK a, ax_temp_mK b);
ax_temp_mK ax_temp_mul(ax_temp_mK a, int32_t scalar);
ax_temp_mK ax_temp_div(ax_temp_mK a, int32_t divisor);
ax_temp_mK ax_temp_clamp(ax_temp_mK val, ax_temp_mK lo, ax_temp_mK hi);

// Mass (int64)
ax_mass_mg ax_mass_add(ax_mass_mg a, ax_mass_mg b);
ax_mass_mg ax_mass_sub(ax_mass_mg a, ax_mass_mg b);
ax_mass_mg ax_mass_mul(ax_mass_mg a, int64_t scalar);
ax_mass_mg ax_mass_div(ax_mass_mg a, int64_t divisor);
ax_mass_mg ax_mass_clamp(ax_mass_mg val, ax_mass_mg lo, ax_mass_mg hi);

// Energy (int64)
ax_energy_mJ ax_energy_add(ax_energy_mJ a, ax_energy_mJ b);
ax_energy_mJ ax_energy_sub(ax_energy_mJ a, ax_energy_mJ b);
ax_energy_mJ ax_energy_mul(ax_energy_mJ a, int64_t scalar);
ax_energy_mJ ax_energy_div(ax_energy_mJ a, int64_t divisor);
ax_energy_mJ ax_energy_clamp(ax_energy_mJ val, ax_energy_mJ lo, ax_energy_mJ hi);
```

### Division Variants

```cpp
// Truncate toward zero (default, matches C++)
int32_t ax_div_trunc_i32(int32_t a, int32_t b);
int64_t ax_div_trunc_i64(int64_t a, int64_t b);

// Floor toward negative infinity
int32_t ax_div_floor_i32(int32_t a, int32_t b);
int64_t ax_div_floor_i64(int64_t a, int64_t b);

// Round to nearest, half away from zero
int32_t ax_div_round_i32(int32_t a, int32_t b);
int64_t ax_div_round_i64(int64_t a, int64_t b);
```

### Cross-Quantity Operations (Deferred)

Operations like `energy = mass * specific_heat * temp_delta` involve multiple
quantities and require careful scale handling. These are **explicitly deferred**
to the first simulation task that needs them (likely TASK-005: Thermal).

TASK-004 establishes the single-quantity foundation only.

### Implementation Location

```
engine/
  core/
    math/
      ax_math_types.h      — type aliases and constants
      ax_math_overflow.h   — overflow policy macros/helpers
      ax_math_safe.h       — safe arithmetic declarations
      ax_math_safe.cpp     — implementations
      ax_math_selftest.h   — selftest checksum declaration
      ax_math_selftest.cpp — selftest implementation
```

---

## C API Additions

Minimal surface for cross-language determinism validation.

### Math Version

```c
#define AX_MATH_VERSION 1u

uint32_t ax_math_get_version(void);
```

Returns `AX_MATH_VERSION`. Bumped on any change to arithmetic behavior.

### Selftest Checksum

```c
uint64_t ax_math_selftest_checksum(uint32_t seed);
```

Runs a deterministic battery of operations across all canonical types:
- Addition (including overflow cases)
- Subtraction (including underflow cases)
- Multiplication (with widening)
- Division (truncate, floor, round)
- Division by zero (saturation path in release)
- Clamping
- Edge cases (min/max values, zero, sign boundaries)

Returns a 64-bit checksum computed from the results.

**Contract:**
- Same `seed` → same checksum, always
- Same checksum from C++ and C# proves determinism
- Checksum algorithm is stable within `AX_MATH_VERSION`

**Behavior:**
- `seed` allows testing different value ranges
- `seed = 0` runs the canonical test battery
- Other seeds provide additional coverage via deterministic value generation

The checksum is computed by XOR-combining intermediate results with position-
dependent mixing (e.g., rotate + XOR). The exact algorithm is implementation-
defined but deterministic.

---

## Static Guarantees

### Type Sizes

C/C++:
```c
static_assert(sizeof(ax_temp_mK) == 4, "ax_temp_mK must be 4 bytes");
static_assert(sizeof(ax_mass_mg) == 8, "ax_mass_mg must be 8 bytes");
static_assert(sizeof(ax_energy_mJ) == 8, "ax_energy_mJ must be 8 bytes");
```

C#:
```csharp
// Validate at startup (C# doesn't have static_assert)
Debug.Assert(sizeof(int) == 4, "ax_temp_mK equivalent must be 4 bytes");
Debug.Assert(sizeof(long) == 8, "ax_mass_mg equivalent must be 8 bytes");
Debug.Assert(sizeof(long) == 8, "ax_energy_mJ equivalent must be 8 bytes");
```

---

## Acceptance Criteria

### C++ (Headless)

**Type definitions:**
- [ ] `ax_temp_mK`, `ax_mass_mg`, `ax_energy_mJ` defined with correct storage
- [ ] Static asserts on sizes pass

**Safe arithmetic (per type):**
- [ ] Add/sub/mul/div work for normal values
- [ ] Overflow detection works in debug (trap or assert)
- [ ] Saturation works in release (clamp to bounds)
- [ ] Division by zero returns saturated value in release
- [ ] Clamp works correctly at boundaries

**Division variants:**
- [ ] `ax_div_trunc` truncates toward zero (7/3=2, -7/3=-2)
- [ ] `ax_div_floor` floors toward -∞ (7/3=2, -7/3=-3)
- [ ] `ax_div_round` rounds to nearest (-7/3=-2, 8/3=3)

**Selftest:**
- [ ] `ax_math_get_version()` returns `AX_MATH_VERSION`
- [ ] `ax_math_selftest_checksum(0)` returns consistent value across runs
- [ ] `ax_math_selftest_checksum(N)` returns different value for different N
- [ ] Checksum exercises all three types and all operations
- [ ] No floating-point operations in selftest or math utilities

### C# (Viewer)

**ABI parity:**
- [ ] `ax_math_get_version()` matches C++ constant
- [ ] `ax_math_selftest_checksum(0)` matches C++ result exactly
- [ ] `ax_math_selftest_checksum(N)` matches C++ for multiple seeds (test N=1,2,3,42,1000)

**Type mapping:**
- [ ] C# uses `int` for temperature, `long` for mass/energy
- [ ] Size assertions pass at startup

### Cross-Language Determinism Proof

The primary acceptance criterion is:

```
For all tested seeds:
  C++ ax_math_selftest_checksum(seed) == C# ax_math_selftest_checksum(seed)
```

This proves identical arithmetic behavior across the ABI boundary.

---

## D005 Update

Upon completion, update `DECISIONS.md` D005 from:

```markdown
**Decision:** TBD (must be selected before non-trivial simulation math is implemented).
```

To:

```markdown
**Decision:** Domain-specific scaled integers.

**Locked Types (v0):**
- Temperature: `ax_temp_mK` (int32, milliKelvin)
- Mass: `ax_mass_mg` (int64, milligram)
- Energy: `ax_energy_mJ` (int64, milliJoule)

**Overflow Policy:**
- Debug: assert/trap
- Release: saturate to type bounds

**Division:** Truncate toward zero by default; explicit floor/round variants available.

**Floating-Point:** Forbidden in math utilities.

**Rationale:**
Domain-specific types are self-documenting and force explicit dimensional handling.
Scaled integers (not universal fixed-point) allow optimal range/precision per quantity.
Integer-only arithmetic guarantees cross-platform determinism.

**Locked by:** TASK-004
```

---

## Completion Notes

TASK-004 locks **DECISIONS.md D005**.

After completion:
- D005 is no longer TBD
- All future simulation tasks may depend on these types and utilities
- Cross-quantity operations remain explicitly deferred to TASK-005+

---

## Follow-On Tasks

- **TASK-005**: First simulation system (thermal diffusion)
  - Will add cross-quantity math as needed (e.g., `energy = mass * specific_heat * temp_delta`)
  - Will use `ax_temp_mK` for cell temperatures
  - Will exercise conservation via `ax_energy_mJ`

- **Future**: Additional quantity types (pressure, volume, flow rate)
  - Added when simulation systems need them
  - Follow same pattern: scaled integer, safe arithmetic, static size asserts
