# Axiom — Unit System

This document defines the **unit system** used by the Axiom simulation core.

It specifies:
- canonical units used internally
- numeric representation and scaling rules
- conservation constraints
- rate integration semantics
- snapshot unit metadata requirements
- responsibilities of the simulation core vs viewer

If a design or implementation choice conflicts with this document,  
**this document wins**.

---

## Core Principle

> **The simulation computes in canonical units.  
> The viewer decides how those units are displayed.**

Unit conversion must never affect simulation outcomes.

---

## Canonical Units (Simulation Truth)

The simulation core uses **SI-based canonical units** exclusively.

Canonical units are:
- deterministic
- platform-independent
- suitable for conservation laws
- independent of player preferences

### Canonical Quantities

| Quantity     | Canonical Unit | Notes |
|-------------|----------------|-------|
| Length      | meters (m)     | Cell size = 1 m |
| Volume      | cubic meters (m³) | Discrete per cell |
| Mass        | kilograms (kg) | Stored as scaled integers |
| Temperature | kelvin (K)     | Absolute temperature |
| Pressure    | pascals (Pa)   | Derived or stored |
| Energy      | joules (J)     | Must be conserved |
| Power       | watts (W)      | J/s |
| Time        | seconds (s)    | Simulation tick = 0.1 s |

No simulation code may assume or depend on display units.

---

## Numeric Representation

### Determinism Requirement

All simulation-relevant quantities must be stored and manipulated using
**deterministic numeric representations**.

Floating-point arithmetic is discouraged in simulation systems unless:
- determinism across platforms is proven
- usage is isolated and documented
- results are invariant under replay

---

### Fixed-Point Policy (Global)

Axiom uses a **global fixed-point policy**.

This policy defines:
- arithmetic rules
- centralized math utilities
- overflow and saturation behavior
- conversion conventions between scales

**This does not imply a single numeric type for all quantities.**

Individual quantities may use different:
- storage widths (e.g., int32 vs int64)
- scale factors (e.g., milliKelvin vs microKelvin)

All quantities must conform to the same **policy**, even if their representations differ.

The fixed-point policy is resolved and locked via **DECISIONS.md D005**.

---

### Example Canonical Storage (Illustrative)

| Quantity        | Example Storage | Rationale |
|-----------------|-----------------|-----------|
| Temperature     | `temp_mK` (int32) | Safe range, simple scaling |
| Liquid Volume   | `volume_mL` (int32) | 1 m³ = 1,000,000 mL |
| Gas Mass        | `mass_mg` (int32 or int64) | Density-dependent |
| Energy          | `energy_mJ` (int64) | Wide accumulation |
| Power           | `power_mW` (int32/int64) | J/s |

Exact choices are implementation-defined but must follow policy.

---

## Conservation Rules

All simulation systems must obey conservation laws.

### Mass Conservation
- Mass may not be created or destroyed implicitly
- All mass sources and sinks must be explicit and auditable
  (e.g., devices, debug commands, world initialization)

### Energy Conservation
- Energy may not be created or destroyed implicitly
- Heat transfer, work, and storage must balance
- Losses (e.g., radiation) must be explicit and parameterized

Any implicit drift is a **bug**, not a tuning issue.

---

## Time and Rates

### Simulation Time

- Simulation tick rate: **10 Hz**
- One tick = **0.1 seconds**
- Time advances only through tick execution

---

### Rate Quantities

Rates are expressed in canonical units **per second**:

| Rate Type     | Canonical Unit |
|--------------|----------------|
| Mass flow     | kg/s |
| Energy flow   | J/s (W) |
| Volume flow   | m³/s |

---

### Rate Integration Over Ticks

During a tick, rates are integrated over:

Δt = 0.1 seconds


Integration must be performed using **deterministic integer arithmetic**
that preserves precision.

The specific method is an implementation decision but must be:
- consistent across all systems
- centrally documented
- free of silent truncation or drift

Acceptable approaches include:
- pre-scaling rates to per-tick quantities
- fixed-point multiplication by Δt
- divide-by-10 arithmetic with explicit remainder handling

Mixing approaches across systems is forbidden.

---

## Derived Quantities

Some quantities are derived rather than stored.

Examples:
- Pressure from gas mass, volume, and temperature
- Density from mass and volume
- Heat energy from temperature and material properties

Rules:
- Derived quantities must be computed deterministically
- If cached, invalidation must be explicit
- **Derived quantities required for display must be computed by the engine**

The viewer must not independently recompute physics-derived values,
even using identical formulas, to avoid divergence if formulas change.

Derived values should be exposed via:
- snapshot channels, or
- read-only query functions

---

## Cross-Type Arithmetic (Forward Reference)

Some computations involve multiple quantities with different scales
(e.g., mass × specific heat × temperature delta).

Rules governing:
- scale promotion
- intermediate widening (e.g., int32 → int64)
- resulting scale factors
- overflow handling

are defined as part of the fixed-point policy resolution  
(see **DECISIONS.md D005**).

No subsystem may define ad-hoc cross-type arithmetic rules.

---

## Snapshot Channel Unit Metadata

Every snapshot channel exposing numeric data must include **unit metadata**.

Metadata includes:
- physical quantity type (e.g., Temperature, Mass, Energy)
- canonical unit (e.g., K, kg, J)
- numeric scale (e.g., milliKelvin, milligrams)
- semantic meaning (absolute vs delta)

The viewer must:
- interpret values using metadata
- perform all unit conversion based on metadata
- avoid hardcoded assumptions tied to channel indices

---

## Display Units (Viewer Responsibility)

The viewer may present canonical values using:
- alternative SI units (°C, °F, kPa, etc.)
- themed or abstracted units
- player-configurable preferences

Rules:
- Display conversion must be reversible for debugging
- Display units must not leak into simulation logic
- Simulation must never branch on display preferences

---

## Queries and Units

Read-only queries returning numeric values must:
- return canonical values
- include or reference unit metadata
- never return display-formatted numbers

If a query begins to influence gameplay outcomes,
it must be promoted to a **command**.

---

## Debug and Development Considerations

Debug commands that inject or modify quantities must:
- use canonical units
- respect conservation rules unless explicitly bypassed
- document any non-physical behavior clearly

Debug visualization may show both canonical and display values.

---

## Versioning

Changes to:
- numeric representations
- scale factors
- unit metadata

are **breaking changes** and must be versioned explicitly.

This applies to:
- save files
- command streams
- snapshot formats

---

## Common Errors (Explicitly Forbidden)

The following are forbidden:
- storing display units in simulation state
- mixing scales for the same quantity
- performing unit conversion inside simulation systems
- allowing UI preferences to affect simulation outcomes
- silent overflow or wraparound in numeric math

---

## Summary

- Canonical units are SI-based and simulation-only
- Numeric storage is deterministic and policy-driven
- Conservation of mass and energy is mandatory
- Rate integration is explicit and consistent
- Derived values are engine-owned
- Snapshot channels carry unit metadata
- Unit changes are versioned, never silent

The unit system is the numeric backbone of Axiom.  
If it is consistent, the simulation remains trustworthy.