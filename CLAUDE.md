# MRTKLIB — AI Development Guide

## 0. Session Start Protocol (Read This First, Every Time)

Before doing anything else, run these three steps:

```bash
cat tasks/todo.md       # What is in progress?
cat tasks/lessons.md    # What mistakes have been made before?
git status              # What is the current state of the working tree?
```

Then state: "I have reviewed todo.md, lessons.md, and git status. Current focus: [active task from todo.md]."
Do not proceed until this is done.

---

## 1. Project Overview

**MRTKLIB** is a modernized, unified GNSS positioning library that integrates JAXA's MALIB, CLASLIB, and MADOCALIB into a single cohesive C/C++ package built on a modern CMake/vcpkg architecture.

### Current Phase: Algorithm Refinement & Real-Time Expansion

All three upstream libraries (MALIB, MADOCALIB, CLASLIB) are fully integrated.
Post-processing and real-time engines are operational for PPP, PPP-AR, PPP-RTK,
and VRS-RTK modes. Current work focuses on algorithm improvements and real-time
feature expansion.

### Version History

| Version | Engine | Improvements | Status |
|---------|--------|-------------|--------|
| **v0.4.1** | RTK | demo5 PAR, `detslp_dop`/`detslp_code`, full-constellation `varerr`, false-fix persistence fix | ✅ Released |
| **v0.4.2** | PPP-RTK, PPP | demo5 `detslp_dop`/`detslp_code`, GLONASS clock guard in `ephpos()`, PAR variance gate + arfilter, full-constellation EFACT, adaptive outlier threshold | ✅ Released |
| **v0.4.3** | PPP-RTK | Real-time CLAS PPP-RTK via `rtkrcv` (BINEX+L6, SBF+L6, RTCM3+UBX; 97.7% fix rate) | ✅ Released |
| **v0.4.4** | PPP-RTK | Dual-channel CLAS real-time via `rtkrcv` | ✅ Released |
| **v0.5.0** | All | TOML configuration (replaces legacy `.conf`) | ✅ Released |
| **v0.5.1** | PPP-RTK | Bug fix: dual-channel CLAS RT fix rate degradation ([#35](https://github.com/h-shiono/MRTKLIB/issues/35)) | ✅ Released |
| **v0.5.2** | All | Code quality: mandatory braces, nested ternary elimination (67 files) | ✅ Released |
| **v0.5.3** | All | Code quality: full `clang-format` application (116 files, Google style) | ✅ Released |
| **v0.5.4** | All | Signals update: frequency / physical band separation and structuring | ✅ Released |
| **v0.5.5** | PPP-RTK | Bug fix: CLAS real-time via UBX ([#31](https://github.com/h-shiono/MRTKLIB/issues/31)) | ✅ Released |
| **v0.5.6** | All | RINEX 4.00 CNAV/CNV2 NAV support (GPS, QZSS, BDS) | ✅ Released |
| **v0.5.7** | — | Port `convbin` and `str2str` CLI applications | ✅ Released |
| **v0.6.0** | All | Unified `mrtk` binary with subcommands; BSS reduced 3 GB → 34 MB | ✅ Released |
| **v0.6.1** | All | Config UX: `systems` list, `excluded_sats`, `taplo` formatter | ✅ Released |
| **v0.6.2** | — | MkDocs Material site + Doxygen API reference + GitHub Pages | ✅ Released |
| **v0.6.3** | PPP-RTK | `mrtk cssr2rtcm3`: real-time CLAS CSSR→RTCM3 MSM4 converter for VRS use | ✅ Released |
| **v0.6.x** | All | Doxygen docstring coverage expansion | 💭 Backlog |

### Test Status

Run `cd build && ctest --output-on-failure` to get current counts. Last known: 59 tests passing.

---

## 2. AI Role — Current Phase

Focus areas in priority order:

1. **Algorithm Improvements:** Port and validate demo5/upstream algorithm refinements. All changes require before/after accuracy comparison.
2. **Real-Time Feature Expansion:** New rtkrcv modes, stream handling, multi-constellation support.
3. **Regression Guarding:** No change is complete without running the full test suite.
4. **Code Quality:** Docstrings, const-correctness, C++ modernization — but only after tests pass.
5. **Upstream Sync:** When MALIB/CLASLIB/MADOCALIB updates are detected, assess impact and cherry-pick selectively.

---

## 3. NEVER DO

These are hard rules. No exceptions, no matter how reasonable it seems in context.

- **NEVER alter GNSS algorithms, matrix operations, or physical constants** during structural/refactoring work unless explicitly instructed.
- **NEVER create `rtcm_t` on the stack or in static arrays** — it is ~103 MB. Always heap-allocate with `calloc`.
- **NEVER mark a task complete without running `ctest --output-on-failure`** and confirming all tests pass.
- **NEVER assume upstream library behavior** — check the actual source when integrating.
- **NEVER silently swallow a test tolerance change** — always flag it to the user with the numerical delta.
- **NEVER push to `main` directly** — all changes go through a feature branch.

---

## 4. Directory Architecture

```
mrtklib/
├── apps/                  # Executable entry points (CLI, GUI)
├── include/mrtklib/       # Public headers
├── src/                   # Core implementation (mrtk_*.c / .cpp)
│   ├── pos/               # Positioning engines (ppp, ppp_ar, ppp_iono, spp, rtkpos, postpos)
│   ├── madoca/            # MADOCA-PPP L6E/L6D decoders
│   ├── models/            # Atmospheric, antenna, tides models
│   ├── data/              # Ephemeris, observation, navigation data handlers
│   ├── rtcm/              # RTCM3 encoder/decoder
│   └── stream/            # Real-time data streams
├── tests/                 # CTest-based regression & unit tests
├── conf/                  # Configuration files
├── tasks/                 # todo.md, lessons.md (always kept current)
└── vcpkg/                 # Dependency management
```

---

## 5. Coding Standards

### Language Standards
- **C++:** C++17 minimum. Use `<filesystem>`, structured bindings, `if constexpr` where appropriate.
- **C:** C11 for legacy-compatible files. Maintain `extern "C"` in headers included by C files.

### Docstrings (mandatory for all public functions)
```cpp
/**
 * @brief Short description.
 * @param[in]  param_name  Description.
 * @param[out] param_name  Description.
 * @return Return value description.
 */
```

### C++ Modernization Rules
- Prefer `std::vector` over raw arrays for new code.
- Use smart pointers (`std::unique_ptr`, `std::shared_ptr`) for new heap allocations.
- Rigorous `const` correctness on all parameters.
- Do not modernize legacy C code in the same commit as algorithm changes.

### Formatting
`.clang-format` is authoritative. Run before committing.

---

## 6. Build & Test Commands

| Action        | Command                                      |
|---------------|----------------------------------------------|
| Configure     | `cmake --preset default`                     |
| Build         | `cmake --build build`                        |
| Test (all)    | `cd build && ctest --output-on-failure`      |
| Test (filter) | `cd build && ctest -R <pattern> --output-on-failure` |
| Coverage      | `cd build && ctest -T Coverage`              |

---

## 7. Key Technical Notes (Accumulated)

### 7.1 rtcm_t Struct Size
`rtcm_t` ≈ 103 MB. Never stack-allocate. Use `calloc` + pointer arrays for multiple instances.

### 7.2 LAPACK vs Embedded LU Solver
MRTKLIB uses system LAPACK; upstream madocalib uses an embedded LU solver. Expect ~1.5–3.8 cm numerical differences in PPP-AR solutions. Test tolerances are adjusted accordingly — never tighten without explicit sign-off.

### 7.3 pppiono_t Design
MRTKLIB: heap-allocated pointer `nav->pppiono = calloc(...)`. Upstream: embedded struct `nav.pppiono`. All access uses `->`, not `.`.

### 7.4 Multi-Channel Support
- **CLAS L6D:** Post-processing and real-time both support dual-channel (`CLAS_CH_NUM=2`). In rtkrcv: stream 1 (base slot) = ch2, stream 2 = ch1.
- **MADOCA L6E:** Post-processing supports multi-L6E via `SSR_CH_NUM=2`. Real-time is single-stream.

### 7.5 Known Bug Patterns (from lessons.md)
- `initx()` placement: must be called before the state is used, not after. Misplacement causes silent filter divergence.
- `filter()` skips zero-initialized states: always explicitly initialize state covariance to a nonzero value before first use.

> *More entries are in `tasks/lessons.md`. Section 7.5 is a summary of the highest-impact patterns only.*

---

## 8. Git Conventions

### Branch Naming
```
feat/<short-description>      # New feature
fix/<short-description>       # Bug fix
test/<short-description>      # Test additions/changes
refactor/<short-description>  # Non-functional cleanup
docs/<short-description>      # Documentation only
```

### Commit Messages (Conventional Commits)
```
feat(ppp): add triple-frequency ambiguity resolution
fix(clas): correct channel assignment in dual-stream mode
test(madoca): add PPP-AR accuracy regression for 2024-01-15
refactor(rtcm): replace raw array with std::vector in msg buffer
docs(api): add doxygen for mrtk_ppppos()
```

### Upstream Sync Policy
When MALIB/CLASLIB/MADOCALIB upstream updates are available:
1. Create branch `sync/<library>-<date>`.
2. Run `git diff upstream/main -- <relevant files>` to identify changes.
3. Classify each change: algorithm (needs sign-off) vs. bugfix (can cherry-pick) vs. formatting (ignore).
4. Cherry-pick only after explicit user approval for algorithm changes.
5. Run full test suite. Document numerical deltas.

---

## 9. Workflow Rules

### 9.1 Bug Fix vs. Design Change
- **Bug fix** (behavior clearly wrong, test is failing): Fix autonomously. Report what you found and what you changed.
- **Design change** (new architecture, new algorithm, new API): Write plan to `tasks/todo.md` and check in with user before implementing.
- When in doubt: it is a design change. Ask.

### 9.2 Plan-First for Non-Trivial Work
For any task with 3+ steps or architectural impact:
1. Write a plan to `tasks/todo.md` with checkable items.
2. Present the plan. Wait for approval.
3. Implement. Mark items complete as you go.
4. After completion, add a review note to `tasks/todo.md`.

### 9.3 Subagent Strategy
Use subagents (Task tool) liberally to keep the main context clean:
- **Research agent:** "Investigate how upstream CLASLIB handles X — summarize findings."
- **Test agent:** "Run the full test suite and report failures with context."
- **Diff agent:** "Compare algorithm in mrtk_ppppos.c against madocalib reference — flag mathematical differences."
- **Docs agent:** "Generate Doxygen docstrings for all undocumented public functions in src/pos/."
- One focused task per subagent. Do not mix concerns.

### 9.4 Self-Improvement Loop
After any user correction:
1. Immediately update `tasks/lessons.md` with the pattern.
2. Check if the same pattern appears anywhere in the current working files.
3. If a high-impact pattern: promote to Section 7.5 of this file.

### 9.5 Definition of Done
A task is done when:
- [ ] All tests pass (`ctest --output-on-failure`)
- [ ] No test tolerances were silently changed
- [ ] Docstrings added for new/modified public functions
- [ ] `tasks/todo.md` updated with completion note
- [ ] Commit message follows convention

---

## 10. Custom Slash Commands

These are defined in `.claude/commands/`. Use them to avoid repetitive setup:

| Command                  | What it does                                                    |
|--------------------------|-----------------------------------------------------------------|
| `/project:test`          | Build + run full ctest, summarize failures                      |
| `/project:review`        | Review staged diff for GNSS algorithm safety (NEVER DO checks) |
| `/project:lessons`       | Prompt to update lessons.md after a correction                  |
| `/project:upstream-diff` | Show diff between MRTKLIB and upstream for a given library      |
| `/project:docgen`        | Generate Doxygen stubs for undocumented functions in a file     |

> *Command files live in `.claude/commands/<name>.md`. Add new ones as patterns emerge.*
