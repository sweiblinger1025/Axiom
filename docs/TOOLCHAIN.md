# Axiom — Toolchain

This document defines how Axiom is built and run.

---

## IDE Setup

- C++: CLion (CMake)
- C#: Visual Studio Community (Viewer) — current project targets .NET 10 (`net10.0`)
- Docs: VS Code

> Note (Windows): CLion may be configured with either MinGW or MSVC toolchains. Both are acceptable for development; MSVC is often nicer for native/managed interop debugging.

---

## Build Requirements

- CMake >= 3.24
- C++20
- .NET 10 (current viewer target)

---

## Build Invocation

```bash
cmake -S . -B build
cmake --build build
