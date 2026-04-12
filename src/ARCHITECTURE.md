# Source Architecture

> Target structure. Legacy code still lives in includes/ and sources/ and will be migrated over time.

## Folder Structure

```
src/
  shared/              — Engine-wide utilities (logger, math, events). No dependencies.
  platform/            — Low-level backend wrappers (GPU abstraction, Vulkan)
    gpu/
  engine/              — Core engine features (rendering, scene, materials)
    rendering/
    scene/
    materials/
  systems/             — Game-level systems (input, audio, physics, networking)
    input/
  app/                 — Temporary test application. Removed when engine ships.
```

## Import Rules

```
app/        → can import from everything
systems/    → engine/, platform/, shared/
engine/     → platform/, shared/
platform/   → shared/
shared/     → nothing (no dependencies)
```

Never import upward. If `platform/gpu/` needs something from `engine/rendering/`, the design is wrong.

## File Convention

Headers (.hpp) and sources (.cpp) live together in the same feature folder. No separate includes/ and sources/ trees.

## Header-Only vs Header + Source

**Header-only (.hpp only):**
- Enums, structs, type definitions
- C++20 concepts
- Small inline functions (1-5 lines)
- Short class methods that fit in the class body
- Templates (must be in headers)

**Header + source (.hpp + .cpp):**
- Functions with non-trivial logic (Vulkan/API calls, branching, error handling)
- Class methods longer than ~5 lines
- Anything that includes heavy headers (VMA, Vulkan) in the implementation

Rule of thumb: if the function body does real work (calls external APIs, has multiple branches, handles errors), it goes in a .cpp. If it's a getter, a small check, or a type definition, it stays header-only.

## Tests

```
tests/
  platform/
    gpu/
  engine/
    rendering/
    scene/
```

Mirrors the src/ structure exactly. Test mocks live alongside their tests.
