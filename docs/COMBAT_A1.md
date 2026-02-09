# COMBAT_A1.md — Shooting Range Contract

**Version:** 0.4  
**Status:** LOCKED  
**Last Updated:** 2026-02-08  
**Depends On:** VISION.md v0.3 (LOCKED), DECISIONS.md (ACTIVE), ARCHITECTURE.md v0.4 (LOCKED), WORLD_INTERFACE.md v0.4 (LOCKED)

---

## Purpose

Define the **implementation contract** for Milestone **A1: Shooting Range**.

A1 is the first playable/provable slice:
- a player can move, look, fire, reload
- static targets take damage and can be destroyed
- save/load works mid-session
- headless runner can replay a scripted action sequence and reproduce **logical outcomes**

This doc is the spec the code must satisfy.

---

## Scope (A1)

In scope:
- single active space
- player character controller (truth-owned by Core)
- hitscan weapon (no projectiles)
- flat damage + target HP
- magazine + reserve ammo
- reload simulated in integer ticks
- per-tick events for damage/reload/target destroy
- snapshot contains all state needed for headless assertions and a basic viewer

Out of scope (A1 non-goals):
- AI enemies (A2)
- cover, stealth, limb damage, VATS
- weapon spread, recoil, ADS sway
- armor, resistances, criticals
- loot/inventory UI (ammo counts only)
- melee
- multiple spaces/interiors (Milestone B)
- physics-driven ragdolls, hit reactions, animation logic in Core (D107)

---

## Decisions (A1)

These are **locked now** and must be added to `DECISIONS.md` as D109–D112.

1) **D109 — Snapshot access pattern:** v1 uses **copy-out snapshots** only (no borrowed views).  
2) **D110 — Hitscan aim source:** the `Fire` action carries no aim vector; Core computes the ray from **truth pose** at tick execution.  
3) **D111 — Reload timing:** reload duration is authored and simulated as **integer ticks** (not seconds).  
4) **D112 — A1 action scheduling usage:** A1 shells submit actions for **next tick only** (`tick = current_tick + 1`). Absolute tick scheduling is supported by the ABI but A1 does not rely on it.

---

## World Setup (A1)

### Space model
- Exactly **one** active space (“range”) in A1.
- No streaming, no interiors.

### Entities
Minimum entities:
- **Player** entity:
  - stable `entity_id` (`uint32_t`)
  - position + rotation (truth)
  - character controller state (grounded, velocity if needed)
  - weapon state (current weapon slot 0, ammo)
- **Target** entities (N ≥ 1):
  - stable `entity_id` (`uint32_t`)
  - position + rotation (truth)
  - `hp` (int32)
  - “destroyed” state flag

Targets are static in A1 (no movement).

### Target archetype
Targets may share the same archetype (content) for A1:
- max HP
- **hit sphere radius** (float) used for ray–sphere intersection

---

## Movement Rules (A1)

A1 movement is intentionally simple and deterministic at the **logic** level.

- **Ground plane:** infinite flat ground at `y = 0`.
- **Gravity:** none in A1 (player cannot fall). The controller clamps `y = 0`.
- **Jumping:** not supported in A1.
- **Walk speed:** fixed constant `walk_speed_m_per_tick` (content-authored or a compile-time constant for A1).
- **Sprinting/crouch:** optional; if implemented, they scale movement speed by fixed multipliers.

`MOVE_INTENT(x,y)` is interpreted as a desired movement direction in player-local space:
- `x` = strafe (-1..+1), `y` = forward (-1..+1)
- Core normalizes/clamps magnitude to 1.0
- Core integrates position once per tick using the current truth rotation (yaw) and the chosen speed

---

## Controls → Actions Mapping (A1)

A1 requires these actions (from `WORLD_INTERFACE.md`):

- `MOVE_INTENT(x,y)`
- `LOOK_INTENT(yaw,pitch)`
- `FIRE_ONCE(weapon_slot=0)`
- `RELOAD(weapon_slot=0)`

Optional but allowed:
- `SPRINT_HELD`
- `CROUCH_TOGGLE`

Input semantics:
- `MOVE_INTENT` is a 2D input vector in player-local space.
- `LOOK_INTENT` is **delta yaw/pitch** applied to the current truth orientation each tick (A1 lock).

Scheduling:
- Shell submits actions for **next tick**.
- Core executes actions at tick boundary.

---

## Weapon Model (A1)

A1 supports a single hitscan weapon (slot 0).

Weapon fields (content / config):
- `magazine_size` (int32)
- `reload_duration_ticks` (uint32)
- `damage_per_hit` (int32)
- `max_range_m` (float) — truth data used in hitscan distance checks; this is **spatial-tier** determinism per D102 (acceptable in v1)

Weapon runtime state (truth):
- `ammo_in_mag` (int32)
- `ammo_reserve` (int32)
- `reloading` (bool)
- `reload_ticks_remaining` (uint32)

---

## Hitscan Rules (A1)

### Ray origin and direction
- Core computes hitscan ray at tick execution using player truth state:
  - **origin:** player position + fixed eye offset (e.g., `(0, eye_height, 0)` in player-local, transformed to world)
  - **direction:** derived from player truth yaw/pitch (look orientation)

A1 does not implement muzzle obstruction; the ray is effectively a “camera ray” derived from truth.

### Hit test
A1 hit testing can start with a simple representation:
- Each target has a **sphere** hit shape with radius `r` (content or constant).
- A hit occurs if the ray intersects the sphere within `max_range_m`.

No material penetration, no ricochet.

### Hit selection
If multiple targets are intersected:
- choose the closest hit along the ray.

---

## Fire Rules (A1)

When executing `FIRE_ONCE` for weapon slot 0:
- If `reloading == true`: no shot fired; emit `FIRE_BLOCKED` event (reason = reloading)
- Else if `ammo_in_mag <= 0`: no shot fired; emit `FIRE_BLOCKED` event (reason = empty_mag)
- Else:
  - decrement `ammo_in_mag` by 1
  - perform hitscan ray test
  - if hit:
    - apply damage to target HP
    - emit `DAMAGE_DEALT` event (attacker=player, target=hit target, value=damage)
    - if target HP becomes <= 0:
      - mark target destroyed
      - emit `TARGET_DESTROY` event

Damage is **flat** (`damage_per_hit`) with no RNG.

---

### Tick ordering (A1)

Within a single tick:
1) Core processes submitted actions for that tick in **batch order** (WORLD_INTERFACE rule).
2) After all actions are processed, Core advances ongoing timers (including reload countdown).

Implication: if `RELOAD` then `FIRE_ONCE` are both submitted for the same tick, the `FIRE_ONCE` sees `reloading == true` and is blocked.

## Reload Rules (A1)

When executing `RELOAD` for weapon slot 0:
- If `reloading == true`: ignore
- Else if `ammo_in_mag == magazine_size`: ignore
- Else if `ammo_reserve <= 0`: ignore
- Else:
  - set `reloading = true`
  - set `reload_ticks_remaining = reload_duration_ticks`
  - emit `RELOAD_STARTED` event

Each tick while `reloading == true`:
- decrement `reload_ticks_remaining` by 1 (integer)
- when it reaches 0:
  - compute `needed = magazine_size - ammo_in_mag`
  - compute `to_load = min(needed, ammo_reserve)`
  - `ammo_in_mag += to_load`
  - `ammo_reserve -= to_load`
  - set `reloading = false`
  - emit `RELOAD_DONE` event

Reload timing is independent of wall-clock time; it is purely tick-based.

---

## Events (A1)

Events are emitted per tick and included in the snapshot event array.

Minimum required events (from `WORLD_INTERFACE.md` plus one A1 addition):

Note: `AX_EVT_FIRE_BLOCKED` is an A1 additive event; `WORLD_INTERFACE.md` is LOCKED, but the implementation must extend `ax_event_type_v1` to include this new enum value.

- `AX_EVT_DAMAGE_DEALT(a=attacker_id, b=target_id, value=damage)`
- `AX_EVT_TARGET_DESTROY(a=attacker_id, b=target_id, value=0)`
- `AX_EVT_RELOAD_STARTED(a=player_id, b=weapon_slot, value=0)`
- `AX_EVT_RELOAD_DONE(a=player_id, b=weapon_slot, value=to_load)`
- `AX_EVT_FIRE_BLOCKED(a=player_id, b=weapon_slot, value=reason_code)` where `reason_code` is a small enum: 1=reloading, 2=empty_mag

Note: snapshot events are **for the current tick only**.

---

## Snapshot Requirements (A1)

A1 snapshot must include, at minimum:
- `tick` counter
- entity list with:
  - player transform (pos + rot quaternion)
  - target transforms (pos + rot quaternion)
  - target HP values (int32)
  - state flags including “is_player”, “is_target”, “is_dead/destroyed”
- player weapon state:
  - `ammo_in_mag`
  - `ammo_reserve`
  - reload state (either ticks remaining or progress)
- event list for the current tick

Snapshot access is copy-out; the shell must be able to parse the blob without additional queries.

---

## Save/Load Requirements (A1)

Save/load must preserve A1 truth state:
- current tick
- player transform + controller state required to continue movement
- weapon state (ammo + reload state, including ticks remaining)
- target states (HP, destroyed)
- any other state required for determinism of the next ticks

Content must be loaded before a save is loaded (ARCHITECTURE rule).

---

## Acceptance Criteria (A1)

A1 is “done” when all are true:

1) **Headless run:** a headless shell can:
   - load content
   - run a scripted sequence for N ticks
   - assert final logical outcomes:
     - ammo counts
     - total damage dealt (**sum of `DAMAGE_DEALT` events**)
     - which targets are destroyed

2) **Deterministic replay (logic):**
   - given identical initial state + identical action sequence, Core produces identical:
     - ammo outcomes
     - target HP outcomes
     - event sequences (types + IDs + values)

3) **Save/load continuity:**
   - saving at tick T, then loading and continuing with the same remaining actions produces identical outcomes to an uninterrupted run.
   - immediately after load, the snapshot at tick T must match the snapshot produced at the moment of save (for the fields A1 cares about: transforms, HP, ammo, reload state).

4) **Basic viewer compatibility (optional but recommended):**
   - a viewer shell can render the player + targets from snapshots and show ammo/HP numbers.

---

## Open Questions (A1-local)

- Eye offset constant: content-authored vs hardcoded for A1.
- Default walk speed: content-authored vs hardcoded for A1.

---

## ChangeLog

### v0.1
- Initial A1 combat contract: single-space shooting range with tick-based reload, truth-derived hitscan, per-tick events, snapshot requirements, and determinism-focused acceptance criteria.

### v0.2
- Promoted A1 implementation choices to locked decisions (D109–D112).
- Added explicit A1 movement rules (flat ground plane, no gravity/jump, fixed speed).
- Locked LOOK intent semantics to delta yaw/pitch for A1.
- Clarified hitscan range as spatial-tier determinism.
- Defined explicit behavior for blocked fire attempts via `FIRE_BLOCKED` events.
- Specified tick-internal ordering (actions first, then timers).
- Tightened acceptance criteria to assert damage totals via events.
- Strengthened save/load invariants to include snapshot equality at save tick.
- Locked target hit shape to a content-authored sphere radius.

### v0.3
- Fixed section merge: separated Movement Rules from Controls → Actions Mapping.
- Added explicit note that `AX_EVT_FIRE_BLOCKED` is an A1 additive event requiring an enum extension in implementation.
- Marked this document as LOCKED.

### v0.4
- Added missing section separator before Weapon Model for formatting consistency.
