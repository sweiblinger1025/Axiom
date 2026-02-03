# Axiom — Spatial Model

Defines the authoritative spatial rules.

---

## Coordinates

- Grid (x, y)
- (0,0) bottom-left
- Y increases upward
- Gravity acts in -Y

---

## Indexing

```
index = y * worldWidth + x
```

---

## Neighbors

- Down, Left, Right, Up (fixed order)

---

## Chunking

- Fixed-size square chunks
- Chunking is internal only

---

## Passability

Terrain defines base.
Occupancy may restrict or override.
