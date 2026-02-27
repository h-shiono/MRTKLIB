# MRTKLIB (Modern RTKLIB) — Development Guide

## 1. Project Overview

**MRTKLIB** is a modernized, unified GNSS positioning library that integrates JAXA's MALIB, claslib, and madocalib into a single cohesive C/C++ package built on a modern CMake/vcpkg architecture.

### Current Phase: MADOCALIB PPP Algorithm Integration

The structural migration from legacy RTKLIB/MALIB to the modern directory layout is **complete**. We are now replacing the PPP algorithm layer with MADOCALIB's implementation. This is a mathematically sensitive phase — algorithm correctness is paramount.

### Phase History

1. ~~Structural Migration~~ — MALIB-based code ported to `include/mrtklib/` + `src/` layout ✅
2. **PPP Engine Swap** — Replacing PPP algorithms with MADOCALIB (current)
3. CLAS Engine Unification — Merging claslib into the unified parameter framework (upcoming)

## 2. Your Role (AI Assistant)

1. **Algorithm Integration:** Port MADOCALIB's PPP logic into the MRTKLIB structure while preserving mathematical correctness down to the bit level.
2. **Build & Compiler Support:** Resolve CMake, vcpkg, and C/C++ compiler issues (include paths, linkage, platform differences).
3. **Documentation:** Generate and maintain Doxygen-style docstrings for all functions, structs, and classes.
4. **Refactoring:** Modernize legacy C code to C++ idioms, but **only after** regression tests for the current state are passing.
5. **Regression Guarding:** Never let a change silently break existing positioning accuracy.

## 3. Directory Architecture

```
mrtklib/
├── apps/          # Executable entry points (CLI, GUI) — separated from core logic
├── include/mrtklib/  # Public headers
├── src/           # Core implementation (mrtk_*.c / .cpp)
├── tests/         # CTest-based regression & unit tests
├── tasks/         # todo.md, lessons.md (project management)
└── vcpkg/         # Dependency management
```

## 4. Coding Standards & Rules

- **No Mathematical Changes (unless explicitly instructed):** Do NOT alter GNSS algorithms, matrix operations, or physical constants during structural work.
- **Docstrings:** Strict Doxygen format:
  ```cpp
  /**
   * @brief Short description of the function.
   * @param[in]  param_name Description of input parameter.
   * @param[out] param_name Description of output parameter.
   * @return Return value description.
   */
  ```
- **C++ Modernization:** Prefer `std::vector` over raw arrays, smart pointers for resource management, rigorous `const` correctness. Maintain `extern "C"` compatibility for headers still included by legacy C files.
- **Formatting:** Assume `.clang-format` is in use. Keep code clean and readable.

## 5. Build Commands

| Action    | Command                                 |
| --------- | --------------------------------------- |
| Configure | `cmake --preset default`                |
| Build     | `cmake --build build`                   |
| Test      | `cd build && ctest --output-on-failure` |

---

## 6. Workflow Orchestration

### 6.1 Plan Node Default
- Enter plan mode for ANY non-trivial task (3+ steps or architectural decisions).
- If something goes sideways, **STOP and re-plan immediately** — don't keep pushing.
- Use plan mode for verification steps, not just building.
- Write detailed specs upfront to reduce ambiguity.

### 6.2 Subagent Strategy
- Use subagents liberally to keep main context window clean.
- Offload research, exploration, and parallel analysis to subagents.
- For complex problems, throw more compute at it via subagents.
- One task per subagent for focused execution.

### 6.3 Self-Improvement Loop
- After ANY correction from the user: update `tasks/lessons.md` with the pattern.
- Write rules for yourself that prevent the same mistake.
- Ruthlessly iterate on these lessons until mistake rate drops.
- Review lessons at session start for relevant project.

### 6.4 Verification Before Done
- Never mark a task complete without proving it works.
- Diff behavior between main and your changes when relevant.
- Ask yourself: "Would a staff engineer approve this?"
- Run tests, check logs, demonstrate correctness.

### 6.5 Demand Elegance (Balanced)
- For non-trivial changes: pause and ask "is there a more elegant way?"
- If a fix feels hacky: "Knowing everything I know now, implement the elegant solution."
- Skip this for simple, obvious fixes — don't over-engineer.
- Challenge your own work before presenting it.

### 6.6 Autonomous Bug Fixing
- When given a bug report: just fix it. Don't ask for hand-holding.
- Point at logs, errors, failing tests — then resolve them.
- Zero context switching required from the user.
- Go fix failing CI tests without being told how.

## 7. Task Management

1. **Plan First:** Write plan to `tasks/todo.md` with checkable items.
2. **Verify Plan:** Check in before starting implementation.
3. **Track Progress:** Mark items complete as you go.
4. **Explain Changes:** High-level summary at each step.
5. **Document Results:** Add review section to `tasks/todo.md`.
6. **Capture Lessons:** Update `tasks/lessons.md` after corrections.

## 8. Core Principles

- **Simplicity First:** Make every change as simple as possible. Impact minimal code.
- **No Laziness:** Find root causes. No temporary fixes. Senior developer standards.
- **Minimal Impact:** Changes should only touch what's necessary. Avoid introducing bugs.