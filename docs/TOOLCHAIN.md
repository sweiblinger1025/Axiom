# Axiom — Toolchain

This document defines how Axiom is built and run.

---

## IDE Setup

- C++: CLion (CMake)
- C#: Visual Studio 2022 (.NET 8)
- Docs: VS Code

---

## Build Requirements

- CMake >= 3.24
- C++20
- .NET 8

---

## Build Invocation (Milestone 0)

```
cmake -S . -B build
cmake --build build
```

---

## Milestone 0

- Build shared library AxiomCore
- Run headless app
- Run C# viewer
- Validate ABI, load/unload, clean exit
