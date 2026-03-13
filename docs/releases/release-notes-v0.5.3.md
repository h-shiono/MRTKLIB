# MRTKLIB v0.5.3 Release Notes

**Release date:** 2026-03-11
**Type:** Code quality (full clang-format)
**Branch:** `chore/clang-format-full`

---

## Overview

v0.5.3 applies `clang-format` to the entire codebase (116 files) to establish
a consistent, machine-enforced code style.  No functional or algorithmic changes.
All 56/56 non-realtime tests pass with identical output.

---

## Changes

### Style — Full clang-format application

All `.c` and `.h` files under `src/`, `apps/`, and `include/` have been
formatted with `clang-format` using the project's `.clang-format` configuration:

```yaml
BasedOnStyle: Google
IndentWidth: 4
ColumnLimit: 120
Language: Cpp
```

Key formatting effects:
- **Consistent indentation** — 4-space indentation uniformly applied
- **Column limit** — Lines wrapped at 120 characters
- **Operator spacing** — Consistent spacing around operators and after keywords
- **Brace placement** — Google-style attached braces
- **Pointer alignment** — Consistent `type *name` formatting
- **Argument alignment** — Multi-line function arguments aligned

### Idempotency verified

Re-running `clang-format --dry-run --Werror` on all files produces zero
violations, confirming the formatting is stable.

---

## Scope

| Category | Count |
|----------|-------|
| Files changed | 116 |
| Lines added | 48,027 |
| Lines removed | 46,636 |
| Net change | +1,391 |

### Included

- `src/**/*.c` — Core library implementation (62 files)
- `include/mrtklib/*.h` — Public headers (47 files)
- `apps/**/*.c` — CLI application sources (7 files)

### Excluded

- `src/core/tomlc99/` — Vendored third-party code (MIT license)
- `apps/cssr2rtcm3/` — Unreleased tool, not yet on `develop`

---

## Tooling

| Tool | Version | Configuration |
|------|---------|---------------|
| clang-format | 21.1.6 (Homebrew LLVM) | `.clang-format` (Google / 4-space / 120-col) |

---

## Test Results

All 56/56 non-realtime tests pass, unchanged from v0.5.2.

| Test Suite | Count | Status |
|------------|-------|--------|
| Unit tests | 12 | PASS |
| SPP / receiver bias | 4 | PASS |
| MADOCA PPP / PPP-AR / PPP-AR+iono | 10 | PASS |
| CLAS PPP-RTK + VRS-RTK | 19 | PASS |
| ssr2obs / ssr2osr / BINEX | 4 | PASS |
| Tier 2 absolute accuracy | 2 | PASS |
| Tier 3 position scatter | 2 | PASS |
| Fixtures | 3 | PASS |
