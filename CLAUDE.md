# MRTKLIB (Modern RTKLIB) — Development Guide

## 1. Project Overview

**MRTKLIB** is a modernized, unified GNSS positioning library that integrates JAXA's MALIB, claslib, and madocalib into a single cohesive C/C++ package built on a modern CMake/vcpkg architecture.

### Current Phase: Algorithm Refinement & Real-Time Expansion

All three upstream libraries (MALIB, MADOCALIB, CLASLIB) are fully integrated.
Post-processing and real-time engines are operational for PPP, PPP-AR, PPP-RTK,
and VRS-RTK modes.  Current work focuses on algorithm improvements and real-time
feature expansion.

### Phase History

1. ~~Structural Migration~~ — MALIB-based code ported to `include/mrtklib/` + `src/` layout ✅
2. ~~PPP Engine Swap~~ — MADOCALIB PPP/PPP-AR/PPP-AR+iono algorithms integrated ✅
3. ~~CLAS Engine Unification~~ — CLASLIB merged into unified parameter framework ✅
4. ~~demo5 Algorithm Porting~~ — RTK (v0.4.1), PPP-RTK/PPP (v0.4.2) ✅
5. ~~Real-Time CLAS PPP-RTK~~ — rtkrcv 1ch (v0.4.3) + 2ch (v0.4.4) ✅

### Test Status (59 tests)

| Test Suite | Count | Status |
|------------|-------|--------|
| Unit tests | 12 | PASS |
| SPP / receiver bias | 4 | PASS |
| rtkrcv real-time (MADOCA + CLAS 1ch + CLAS 2ch) | 3 | PASS |
| MADOCA PPP / PPP-AR / PPP-AR+iono | 10 | PASS |
| CLAS PPP-RTK + VRS-RTK | 19 | PASS |
| ssr2obs / ssr2osr / BINEX | 4 | PASS |
| Tier 2 absolute accuracy | 2 | PASS |
| Tier 3 position scatter | 2 | PASS |
| Fixtures | 3 | PASS |

## 2. Your Role (AI Assistant)

1. **Algorithm Integration:** Port remaining claslib logic into the MRTKLIB structure while preserving mathematical correctness.
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
│   ├── pos/       # Positioning engines (ppp, ppp_ar, ppp_iono, spp, rtkpos, postpos)
│   ├── madoca/    # MADOCA-PPP L6E/L6D decoders (mrtk_madoca.c, mrtk_madoca_iono.c)
│   ├── models/    # Atmospheric, antenna, tides models
│   ├── data/      # Ephemeris, observation, navigation data handlers
│   ├── rtcm/      # RTCM3 encoder/decoder
│   └── stream/    # Real-time data streams
├── tests/         # CTest-based regression & unit tests
├── conf/          # Configuration files (madocalib/, etc.)
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

## 6. Key Technical Notes

### 6.1 rtcm_t Struct Size
`rtcm_t` is approximately **103 MB**. Never create static arrays of `rtcm_t`. Use heap allocation (`calloc`) and pointer arrays when multiple instances are needed.

### 6.2 LAPACK vs Embedded LU Solver
MRTKLIB uses system LAPACK; upstream madocalib uses an embedded LU solver. This causes ~1.5-3.8cm numerical differences in PPP-AR solutions. Test tolerances are adjusted accordingly.

### 6.3 pppiono_t Design
MRTKLIB uses a heap-allocated pointer (`nav->pppiono = calloc(...)`) while upstream uses an embedded struct (`nav.pppiono`). All access uses `->` instead of `.`.

### 6.4 Multi-Channel Support
- **CLAS L6D**: Both post-processing and real-time support dual-channel (`CLAS_CH_NUM=2`).  In rtkrcv, stream 1 (base slot) carries ch2, stream 2 carries ch1.
- **MADOCA L6E**: Post-processing supports multi-L6E via `SSR_CH_NUM=2`.  Real-time is single-stream (receiver typically multiplexes into one SBF stream).

---

## 7. Workflow Orchestration

### 7.1 Plan Node Default
- Enter plan mode for ANY non-trivial task (3+ steps or architectural decisions).
- If something goes sideways, **STOP and re-plan immediately** — don't keep pushing.
- Use plan mode for verification steps, not just building.
- Write detailed specs upfront to reduce ambiguity.

### 7.2 Subagent Strategy
- Use subagents liberally to keep main context window clean.
- Offload research, exploration, and parallel analysis to subagents.
- For complex problems, throw more compute at it via subagents.
- One task per subagent for focused execution.

### 7.3 Self-Improvement Loop
- After ANY correction from the user: update `tasks/lessons.md` with the pattern.
- Write rules for yourself that prevent the same mistake.
- Ruthlessly iterate on these lessons until mistake rate drops.
- Review lessons at session start for relevant project.

### 7.4 Verification Before Done
- Never mark a task complete without proving it works.
- Diff behavior between main and your changes when relevant.
- Ask yourself: "Would a staff engineer approve this?"
- Run tests, check logs, demonstrate correctness.

### 7.5 Demand Elegance (Balanced)
- For non-trivial changes: pause and ask "is there a more elegant way?"
- If a fix feels hacky: "Knowing everything I know now, implement the elegant solution."
- Skip this for simple, obvious fixes — don't over-engineer.
- Challenge your own work before presenting it.

### 7.6 Autonomous Bug Fixing
- When given a bug report: just fix it. Don't ask for hand-holding.
- Point at logs, errors, failing tests — then resolve them.
- Zero context switching required from the user.
- Go fix failing CI tests without being told how.

## 8. Task Management

1. **Plan First:** Write plan to `tasks/todo.md` with checkable items.
2. **Verify Plan:** Check in before starting implementation.
3. **Track Progress:** Mark items complete as you go.
4. **Explain Changes:** High-level summary at each step.
5. **Document Results:** Add review section to `tasks/todo.md`.
6. **Capture Lessons:** Update `tasks/lessons.md` after corrections.

## 9. Core Principles

- **Simplicity First:** Make every change as simple as possible. Impact minimal code.
- **No Laziness:** Find root causes. No temporary fixes. Senior developer standards.
- **Minimal Impact:** Changes should only touch what's necessary. Avoid introducing bugs.
