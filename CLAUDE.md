# MRTKLIB — AI Development Guide

## 0. Session Start Protocol

Before doing anything else, run these checks:

```bash
git status                              # current working tree
[ -f tasks/todo.md ] && cat tasks/todo.md           # active task (maintainer-local; absent in clones)
[ -f tasks/lessons.md ] && cat tasks/lessons.md     # cumulative lessons (maintainer-local; absent in clones)
```

Then state: "I have reviewed [files actually read]. Current focus: [active task, or 'no maintainer-local task state, will rely on git status and the user prompt']."

`tasks/` is gitignored. In a fresh clone the AI proceeds without it; in the maintainer's environment those files are the canonical session-handoff state and must be read.

---

## 1. Project Overview

**MRTKLIB** is a modernized, unified GNSS positioning library that integrates JAXA's MALIB, CLASLIB, and MADOCALIB into a single cohesive C/C++ package on a CMake/vcpkg foundation.

### Current Phase: Algorithm Refinement & Real-Time Expansion

All three upstream libraries (MALIB, MADOCALIB, CLASLIB) are fully integrated. Post-processing and real-time engines are operational for PPP, PPP-AR, PPP-RTK, and VRS-RTK modes. Current work focuses on algorithm refinement and real-time feature expansion.

### Recent Releases

See [`docs/releases/changelog.md`](docs/releases/changelog.md) for the full history. Recent highlights:

| Version | Theme |
|---------|-------|
| v0.5.x | TOML configuration, code quality sweeps, signals restructuring, RINEX 4.00 CNAV, `convbin` / `str2str` ports |
| v0.6.0 | Unified `mrtk` binary with subcommands (BSS reduced 3 GB → 34 MB) |
| v0.6.1 | Config UX: `systems` list, `excluded_sats`, `taplo` formatter |
| v0.6.2 | MkDocs Material site + Doxygen API reference + GitHub Pages |
| v0.6.3 | NTRIP v2 (HTTP/1.1) protocol support |
| v0.6.4 | rtkrcv status-path stability + GitHub Community Profile |
| v0.6.5 | First official `mrtk cssr2rtcm3` release + 24-hour endurance test |
| v0.6.6 | Unified subcommand help format + GNU-style long-option aliases |
| v0.6.7 | IGS-products float PPP via `correction = "igs"` |
| v0.6.8 | Real-time IGS-RTS float PPP via `correction = "igs-rts"` |
| v0.6.9 | IGS-products integer PPP-AR (Bias-SINEX OSB phase biases) |
| v0.6.10 | SPP accuracy (opt-in): C/N0 weighting, IGG-III robust, TDCP |
| v0.6.11 | Developer experience: formatter CI gate + repo-wide format baseline, GSDC smartphone SPP benchmark, faster CI (no positioning change) |
| v0.6.12 | Real-time MADOCA-PPP multi-GNSS signal selection (Galileo / QZSS / GLONASS L2C/A) |
| v0.6.13 | Enhanced PPP-RTK SPP seed (C/N0 + TDCP) + RINEX converter obsdef frequency unification |
| v0.6.14 | Real-time CLAS handover robustness + cssr2rtcm3 / VRS carrier-phase quality |
| v0.7.0 | Galileo HAS (E6-B C/NAV) float PPP — opens the v0.7.x global SSR correction services series |
| v0.7.1 | Two-receiver QZSS L6 (mosaic-CLAS / mosaic-G5) + single-SBF MADOCA-PPP |
| v0.7.2 | CLAS PPP-RTK accuracy/robustness fixes + `mrtk dump --parse` (upstream-identical parse_cssr_*.csv) |
| v0.7.3 | Real-time MADOCA-PPP PPP-AR/IONO — L6D wide-area ionospheric augmentation (QZS PRN 200/201) wired into `mrtk run` |
| v0.7.4 | CLAS PPP-RTK claslib parity (#225) — storm-day fix 89.44%→93.54% (> upstream 92.88%); BackupCSSR gap-bridge, GAL E5a PCV fix; opt-in `MRTK_DETERMINISTIC_BLAS` |
| v0.7.5 | Single-band CLAS PPP-RTK regression coverage (SF nf=1 + Galileo E1/E5b mismatch, independent-truth anchors) + capability record `docs/design/sf-ppp-rtk.md`; no positioning change |

### Test Status

Run `cd build && ctest --output-on-failure` to get current counts. Last known: 79 tests, 77 passing — 2 perennial env failures (`rtkrcv_rt` headless RT replay; `madocalib_pppar_ion_check` LAPACK-vs-reference ~1.6 cm vs 0.5 cm tol), both reproduce on a clean `develop` build and are unrelated to code changes.

---

## 2. AI Role — Current Phase

Focus areas, in priority order:

1. **Algorithm improvements.** Port and validate demo5 / upstream algorithm refinements. All numerical changes require before/after accuracy comparison.
2. **Real-time feature expansion.** New rtkrcv modes, stream handling, multi-constellation support.
3. **Regression guarding.** No change is complete without running the full test suite.
4. **Code quality.** Docstrings, const-correctness, modernization — but only after tests pass.
5. **Upstream sync.** When MALIB / CLASLIB / MADOCALIB updates are detected, assess impact and cherry-pick selectively.

---

## 3. NEVER DO

Hard rules. No exceptions, no matter how reasonable an action seems in context.

- **NEVER alter GNSS algorithms, matrix operations, or physical constants** during structural / refactoring work unless explicitly instructed.
- **NEVER create `rtcm_t` on the stack or in static arrays** — it is ~103 MB. Always heap-allocate with `calloc`. See §7.1 for the size table of other large structs.
- **NEVER mark a task complete without running `ctest --output-on-failure`** and confirming all tests pass.
- **NEVER assume upstream library behavior** — read the actual source when integrating.
- **NEVER silently swallow a test tolerance change** — flag it to the user with the numerical delta.
- **NEVER push to `main` directly** — all changes go through a feature branch.
- **NEVER commit changes without an explicit user request.** "Looks good, ship it" or equivalent is required.
- **NEVER skip pre-commit hooks** (`--no-verify`, `--no-gpg-sign`) or bypass signing. Hook failures must be root-caused, not bypassed.
- **NEVER run destructive git operations without explicit confirmation:** `git push --force`, `git reset --hard`, `git branch -D`, `git filter-branch`, `git clean -fdx`, history rewrites. When in doubt, ask before acting.

---

## 4. Directory Architecture

```
mrtklib/
├── apps/                  # Executable entry points (CLI, GUI)
├── include/mrtklib/       # Public headers
├── src/                   # Core implementation (mrtk_*.c / .cpp)
│   ├── pos/               #   Positioning engines (ppp, ppp_ar, ppp_iono, spp, rtkpos, postpos)
│   ├── madoca/            #   MADOCA-PPP L6E/L6D decoders
│   ├── models/            #   Atmospheric, antenna, tide models
│   ├── data/              #   Ephemeris, observation, navigation data handlers
│   ├── rtcm/              #   RTCM3 encoder/decoder
│   └── stream/            #   Real-time data streams
├── tests/                 # CTest-based regression & unit tests
├── conf/                  # Configuration files (TOML)
├── docs/                  # User-facing docs (MkDocs); docs/dev/ for developer references
├── vcpkg/                 # Dependency management
├── tasks/                 # Maintainer-local task tracker & lessons (gitignored)
└── .claude/               # Maintainer-local Claude Code config (gitignored)
```

`tasks/` and `.claude/` are gitignored. A fresh clone of the repository will not contain them. Anything that needs to be visible to external contributors lives under `docs/`, `include/`, `src/`, `tests/`, `conf/`, or in the project README.

---

## 5. Coding Standards

### Language

- **C:** C11 for new code. The library target compiles with `-std=c11`; the global `-ansi` flag is overridden per-target (see [`docs/dev/pitfalls-public.md`](docs/dev/pitfalls-public.md) P-01).
- **C++:** C++17 for new C++ files (currently a small fraction of the codebase). Use `<filesystem>`, structured bindings, `if constexpr` where they help readability.
- **Headers consumed by C:** maintain `extern "C"` blocks.

### Docstrings (mandatory for public API)

```c
/**
 * @brief Short description.
 * @param[in]  param_name  Description.
 * @param[out] param_name  Description.
 * @return Return value description.
 */
```

### Formatting

Pinned tool versions (must match CI and `CONTRIBUTING.md`): clang-format **21.1.6**, taplo **0.10.0**, ruff **0.15.2**.

- **C / C++:** `.clang-format` (Google base) is authoritative. Run before committing. Vendored / upstream-derived code (`src/core/tomlc99`, `util/`) is excluded via `.clang-format-ignore` — do not reformat it.
- **TOML:** `taplo` is the formatter, configured by `taplo.toml` at the repo root. Run `taplo fmt` (or use the VS Code "Even Better TOML" extension) on any edit under `conf/`, root-level config files, or new TOML samples before committing.
- **Python:** `ruff format` is the formatter, configured by `ruff.toml`. Run `ruff format` on any edit under `scripts/` before committing. `ruff check` (lint) is a separate concern, **not** part of the formatting gate.

### Style — what NOT to add

These hurt the codebase. Don't do them even if they seem helpful:

- **Don't write comments that restate the code.** Comments are for the *why* when it's non-obvious — a hidden constraint, a subtle invariant, a workaround for a specific bug. If a comment would only say what the code already says, omit it.
- **Don't add backwards-compatibility shims** (renamed-and-kept-old-name, deprecated aliases) unless the user explicitly asks. The codebase has no external API stability guarantee outside `include/mrtklib/`.
- **Don't add error handling for cases that can't happen.** Trust internal code and framework guarantees. Validate at system boundaries (user input, file parsing, network), not between trusted internal modules.
- **Don't introduce abstractions for hypothetical future requirements.** Three similar lines is preferable to a premature template / factory / strategy pattern. Only abstract when the third or fourth concrete case appears.
- **Don't modernize legacy C in the same commit as an algorithm change.** Two separate commits, each independently reviewable.

---

## 6. Build & Test Commands

| Action        | Command                                                |
|---------------|--------------------------------------------------------|
| Configure     | `cmake --preset default`                               |
| Build         | `cmake --build build`                                  |
| Test (all)    | `cd build && ctest --output-on-failure`                |
| Test (filter) | `cd build && ctest -R <pattern> --output-on-failure`   |
| Coverage      | `cd build && ctest -T Coverage`                        |

For long ctest runs (regression tests can take several minutes), prefer launching a `test-summarizer` subagent so the main context is not blocked. See §9.3.

### Unified `mrtk` binary

Since v0.6.0 the historical separate binaries (`rtkrcv`, `rnx2rtkp`, `convbin`, `str2str`, …) are consolidated into a single `mrtk` executable with subcommands. Use the unified form in new code, scripts, documentation, and configuration:

| Subcommand   | Replaces / role                                          |
|--------------|----------------------------------------------------------|
| `mrtk run`   | `rtkrcv` — real-time positioning pipeline                |
| `mrtk post`  | `rnx2rtkp` — post-processing positioning                 |
| `mrtk relay` | `str2str` — relay and split data streams                 |
| `mrtk convert` | `convbin` — raw → RINEX conversion                    |
| `mrtk cssr2rtcm3` | Real-time CLAS CSSR → RTCM3 MSM converter (VRS)     |
| `mrtk l6extract` | Extract L6 frames from SBF/UBX to per-PRN files       |
| `mrtk ssr2obs` / `ssr2osr` / `bias` / `dump` | utilities                |

Run `./build/mrtk --help` for the current full list and per-subcommand help via `./build/mrtk <sub> --help`. The legacy binaries are no longer built.

---

## 7. Key Technical Notes (Accumulated)

### 7.1 Large struct sizes — always heap-allocate

| Type | Approximate size |
|------|-----------------|
| `rtksvr_t` | ~972 MB |
| `rtcm_t` | ~103 MB |
| `has_t` | ~1.6 MB |
| `osb_t` | ~700 KB |
| `clas_corr_t` | ~352 KB |

Never stack-allocate or place these in static arrays. Use `calloc()` and pass by pointer. For multiple instances, use an array of pointers (not an array of structs).

### 7.2 LAPACK vs embedded LU solver

MRTKLIB links system LAPACK; upstream MADOCALIB uses an embedded LU solver. Expect ~1.5–3.8 cm numerical differences in PPP-AR solutions. Test tolerances are adjusted accordingly — **never tighten without explicit sign-off**.

### 7.3 `pppiono_t` design

MRTKLIB uses a heap-allocated pointer: `nav->pppiono = calloc(...)`. Upstream embeds the struct as `nav.pppiono`. All MRTKLIB access uses `->`, not `.`.

### 7.4 Multi-channel support

- **CLAS L6D:** Both post-processing and real-time support dual-channel (`CLAS_CH_NUM=2`). In rtkrcv, stream 1 (base slot) = ch2, stream 2 = ch1.
- **MADOCA L6E:** Post-processing supports multi-L6E via `SSR_CH_NUM=2`. Real-time is single-stream.

### 7.5 Known pitfalls

The repeatable pitfall patterns are catalogued in [`docs/dev/pitfalls-public.md`](docs/dev/pitfalls-public.md). Topics covered:

- Build & toolchain (global `-ansi` flag, `BLA_SIZEOF_INTEGER` hint vs. guarantee, `trace()` no-op)
- Memory & allocation (`MAXSAT` vs `MAXOBS`, `glorbit()` non-convergence)
- GNSS algorithm pitfalls (frequency slot hardcoding, `nav->eph[]` direct indexing, GAL F/NAV slot, MSM signal-ID tables, MSM encoder semantics, CLAS `nf=2`)
- Platform & runtime (macOS `cu.*` serial, diagnosing stuck daemons)
- CSSR / IS-QZSS-L6 subtype reference

Read that file when touching any of these areas, and consult it before commit when running `/review`. The maintainer keeps a longer internal investigation log; the public catalog distils the reusable rules.

### 7.6 BLAS/LAPACK LP64 ABI

MRTKLIB requires an **LP64** BLAS/LAPACK provider (32-bit integer interface). On CMake ≥ 3.22, `CMakeLists.txt` sets `BLA_SIZEOF_INTEGER=4` as a request to `FindBLAS.cmake` and rejects an explicit `=8` override with `FATAL_ERROR`.

This is a *hint*, not enforcement: `FindBLAS.cmake` uses `BLA_SIZEOF_INTEGER` to pick between library-name candidates (e.g. `openblas` vs `openblas64`), but does not verify the resolved library's ABI. On platforms where the ILP64 build is shipped under the LP64 filename (NixOS being one), an ILP64 library can still be linked silently. Crashes in `utest_t_matrix` or any LAPACK call are the canonical symptom; the remediation is to pass an explicit LP64 provider via `CMAKE_PREFIX_PATH` or `BLAS_LIBRARIES`.

Documentation and code comments should use *requests* / *hint* wording rather than *enforces* / *forces*.

### 7.7 Antenna PCV array width: `NFREQPCV`, not `NFREQ`

Two constants in `include/mrtklib/mrtk_foundation.h`:

- `NFREQ = 3`     — number of carrier frequencies used by the positioning engines
- `NFREQPCV = 12` — number of carrier frequencies stored in `pcv_t` (antenna phase-center parameters)

`antmodel()` and `antmodel_s()` in `src/models/mrtk_antenna.c` loop `for (i = 0; i < NFREQPCV; i++)` and write **12 elements** into the output `double *dant` argument. The function signature gives no size hint, and the docstring only says "range offsets for each frequency".

Callers that allocate `double dant[NFREQ]` (= 3 elements) and pass it to `antmodel()` will overflow the buffer by 9 doubles. The symptom is silent stack/heap corruption that only triggers when a non-zero PCV value is written into one of the upper slots.

**Rule:** When calling `antmodel()` / `antmodel_s()`, the output buffer must be sized `NFREQPCV` (or `MAXFREQ`, whichever is in use locally). Audit any new caller for this. Do not change the loop bound to `NFREQ` without auditing every caller of `pcv_t`-aware code first.

---

## 8. Git Conventions

### Branch naming

```
feat/<short-description>      # New feature
fix/<short-description>       # Bug fix
test/<short-description>      # Test additions/changes
refactor/<short-description>  # Non-functional cleanup
docs/<short-description>      # Documentation only
sync/<library>-<date>         # Upstream sync
```

### Commit messages (Conventional Commits)

```
feat(ppp): add triple-frequency ambiguity resolution
fix(clas): correct channel assignment in dual-stream mode
test(madoca): add PPP-AR accuracy regression for <data>
refactor(rtcm): replace raw array with std::vector in msg buffer
docs(api): add doxygen for mrtk_ppppos()
```

### Upstream sync policy

When MALIB / CLASLIB / MADOCALIB upstream updates are available:

1. Create branch `sync/<library>-<date>`.
2. Run `git diff upstream/main -- <relevant files>` to identify changes.
3. Classify each hunk: algorithm (needs sign-off) / bugfix (can cherry-pick) / formatting (ignore).
4. Cherry-pick only after explicit user approval for algorithm changes.
5. Run the full test suite. Document numerical deltas.

---

## 9. Workflow Rules

### 9.1 Bug fix vs. design change

- **Bug fix** (behaviour clearly wrong, test failing): fix autonomously. Report what was found and what changed.
- **Design change** (new architecture, new algorithm, new API surface): write a plan and check in before implementing.
- When in doubt: it is a design change. Ask.

### 9.2 Planning artefacts

Three different planning surfaces, each with its own role:

| Surface | Role | Lifetime |
|---------|------|----------|
| **Plan mode** (`ExitPlanMode`) | Pre-implementation alignment with the user on the *approach* | This message |
| **`TodoWrite`** | In-session progress tracking across multi-step work | This session |
| **`tasks/todo.md`** (maintainer-local) | Cross-session task continuity, hand-off between sessions | Until the task ships |

For any task with 3+ steps or architectural impact:

1. If the approach is non-obvious, present it through Plan mode and wait for approval.
2. Use `TodoWrite` to track sub-steps during the session.
3. The maintainer updates `tasks/todo.md` as the canonical record of "what is still open" between sessions.

### 9.3 Subagent strategy

The `Agent` tool has real costs: the subagent has no context from the current conversation (cold-start briefing required), its findings are lossy-compressed into a single return message, spawn latency is fixed, and it cannot ask the user a clarifying question mid-task. Use it selectively, not by default.

**Use a subagent when:**

- The work is long-running (full ctest, large build, multi-file research) and would block the main response
- The task is open-ended exploration that would consume significant context (e.g. "find every place X is referenced and classify each")
- Multiple independent investigations can run in parallel — issue them in a **single message with multiple `Agent` tool uses** so they execute concurrently
- The task matches a pre-configured specialised agent (algorithm safety review, test summarizer, upstream classifier)

**Do NOT use a subagent when:**

- The target file path or symbol is already known — use `Read` / `Grep` / `Glob` directly
- The briefing cost (writing a self-contained prompt) exceeds the context the subagent would save
- The task needs intermediate clarification back to the user
- A direct edit / single-file change is what's actually required

**Typical specialised shapes** (project conveniences live under `.claude/agents/`, gitignored; external contributors can configure their own):

- Algorithm safety review for diffs under `src/pos/`, `src/clas/`, `src/madoca/`, `src/models/`
- Test suite runner for long ctest invocations (the run takes minutes; main context stays free)
- Upstream diff classifier for sync branches touching many files

One focused task per subagent — do not mix concerns. When you delegate, do not duplicate the same search yourself.

### 9.4 Self-improvement loop

After any user correction:

1. Update the maintainer-local lessons log (`tasks/lessons.md`) immediately.
2. Check whether the same pattern appears in the current working files.
3. If the pattern is high-impact and generalisable, propose promotion to [`docs/dev/pitfalls-public.md`](docs/dev/pitfalls-public.md) per the local visibility policy. If it would belong in everyone's session, propose adding it to §7 of this file.

### 9.5 Definition of Done

A task is done when:

- [ ] All tests pass (`ctest --output-on-failure`)
- [ ] No test tolerances were silently changed
- [ ] Doxygen docstrings exist for new / modified public functions
- [ ] Maintainer task tracker reflects completion
- [ ] Commit message follows Conventional Commits
- [ ] No unrelated changes are bundled into the commit

---

## 10. Tooling

### Slash commands and skills

The maintainer's `.claude/` directory (gitignored) contains project-specific Claude Code commands and agents. They are conveniences for the maintainer's workflow, not a public API surface. External contributors using Claude Code can register equivalent commands in their own workspace.

Examples of the patterns covered:

| Pattern | What it does |
|---------|--------------|
| `/test` | Build + run full ctest, summarize failures |
| `/review` | Review staged diff against §3 NEVER DO + [`docs/dev/pitfalls-public.md`](docs/dev/pitfalls-public.md) |
| `/upstream-diff` | Compare MRTKLIB against upstream for a given library |
| `/docgen` | Generate Doxygen stubs for undocumented functions in a file |

### Tool discipline

- Prefer dedicated tools over `Bash` when one fits: `Read` for files, `Edit` / `Write` for modifications, `Grep` / `Glob` for search.
- Issue independent tool calls in parallel — sequence them only when the next call depends on the previous result.
- Brief status updates at key moments (start of work, on finding, on direction change, on blocker). Keep the main response short; the diff and the test result speak for themselves.
