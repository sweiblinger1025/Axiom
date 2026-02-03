# Axiom — Command Model

Defines how truth changes.

---

## Core Rule

All state changes occur via commands.

---

## Lifecycle

1. Create
2. Submit (structural validation)
3. Process at tick (validate → apply)
4. Report result

---

## Semantics

- Sequential application
- Engine-assigned IDs
- Non-transactional batching
- Replayable

---

## Debug Commands

- Same pipeline
- Gated, auditable
