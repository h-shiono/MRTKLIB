# Contributing to MRTKLIB

Thanks for your interest in MRTKLIB. This document explains how to report
issues, propose changes, and get a pull request merged.

If you are just looking for how to *use* MRTKLIB, start with [README.md](README.md)
and the [documentation site](https://h-shiono.github.io/MRTKLIB/).

---

## What contributions fit this project

MRTKLIB is a GNSS positioning library focused on QZSS augmentation services
(MADOCA-PPP, CLAS PPP-RTK, VRS-RTK) and the surrounding real-time and
post-processing workflows. We welcome contributions that:

- Fix bugs in positioning engines, decoders, streams, or build tooling
- Improve the TOML configuration UX or CLI ergonomics
- Add or strengthen test coverage, especially regression datasets
- Improve documentation (Doxygen, MkDocs, examples, error messages)
- Back-port well-understood algorithm improvements from upstream RTKLIB
  forks (e.g. demo5, MALIB, MADOCALIB, CLASLIB)

Contributions that materially change the mathematics of positioning
algorithms are welcome but require regression evidence — see
[Positioning-regression guard](#positioning-regression-guard) below.

---

## Reporting issues

Open a new issue at
<https://github.com/h-shiono/MRTKLIB/issues/new/choose>. Five templates are
available; pick the one that matches your situation:

| Template | Use when |
|----------|----------|
| **Bug report** | Build failure, crash, or incorrect behavior unrelated to positioning accuracy |
| **Positioning accuracy / Fix-rate issue** | Solution is wrong or degraded (Fix rate, accuracy, convergence time) |
| **Feature request** | New feature, enhancement, or refactor proposal |
| **Documentation issue** | Errors, omissions, or confusing wording in the docs |
| **Question / usage help** | How to use MRTKLIB or interpret results |

### What makes a good positioning-issue report

Positioning problems are hard to diagnose without concrete data. The
template will ask for:

- **Mode**: SPP / RTK / PPP / PPP-AR / PPP-RTK / VRS-RTK
- **Correction source**: CLAS (L6D), MADOCA (L6E), MADOCA-PPP, or none
- **Constellations enabled**: e.g. GPS + Galileo + QZSS
- **Dataset**: observation/correction files, duration, date, location, and
  whether the data can be shared
- **Observed vs. expected**: Fix rate, horizontal/vertical accuracy,
  time-to-first-fix, longest continuous Fix
- **Baseline for regression**: if this is a regression, which commit or
  version produced the expected result
- **Exact command line** you ran

Screenshots of ENU plots or scatter plots help a lot. Attach logs as files
rather than pasting huge outputs inline.

### Security vulnerabilities

Please do **not** open public issues for security vulnerabilities. See
[SECURITY.md](SECURITY.md) for the private disclosure channel.

---

## Before you open a pull request

### 1. Discuss first for non-trivial changes

If you plan to change positioning mathematics, rename public APIs, or
restructure directories, please open an issue first and get rough alignment
before writing code. For small bug fixes and documentation improvements,
feel free to go straight to a PR.

### 2. Fork and branch

Fork the repository on GitHub, then create a topic branch off `develop`:

```bash
git checkout develop
git pull origin develop
git checkout -b feat/short-descriptive-name
```

Branch naming conventions used in this repo:

| Prefix | Use for |
|--------|---------|
| `feat/` | New features or enhancements |
| `fix/` | Bug fixes |
| `refactor/` | Internal code changes with no behavior change |
| `docs/` | Documentation-only changes |
| `ci/` | CI / build / tooling changes |
| `test/` | Test-only additions or fixes |

### 3. Build and test locally

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cd build && ctest --output-on-failure
```

All 62 tests must pass before you open a PR. If your change makes a test
fail intentionally, update the test in the same commit and explain why in
the commit message.

### 4. Follow the coding standards

Project-wide rules are in [CLAUDE.md §4](CLAUDE.md). The headline items:

- **No drive-by math changes.** Do not alter GNSS algorithms, matrix
  operations, or physical constants as part of unrelated work. Isolate
  algorithm changes to their own commits with regression evidence.
- **Doxygen on public functions**:

  ```c
  /**
   * @brief Short description of the function.
   * @param[in]  param_name Description of input parameter.
   * @param[out] param_name Description of output parameter.
   * @return Return value description.
   */
  ```

- **`clang-format`** is the source of truth for formatting. Run it on
  your changes before committing.
- **`extern "C"`** compatibility on headers that are still consumed by
  legacy C files.
- **`const`-correctness** on pointer parameters that are not written to.
- Modern C++ idioms (`std::vector`, smart pointers) are welcome in new
  code, but keep the public C API stable.

### 5. Commit messages

Use conventional-style prefixes matching the branch taxonomy:

```
feat(rtkrcv): add dual-stream L6D routing by PRN
fix(clas): guard trop bank against stale entries past age threshold
refactor(toml): extract signal-list parsing into helper
docs(readme): document the cssr2rtcm3 release status
ci(github): pin label-sync action to a commit SHA
test(ppp-rtk): add regression for Galileo E1C/E5Q signal fallback
```

Keep the subject line under ~72 characters. Use the body to explain *why*
the change is needed, especially for algorithm changes or subtle bug fixes.

---

## Pull request process

### 1. Open the PR against `develop`

`main` is the released branch; all feature work integrates via `develop`
first. PRs against `main` are reserved for release promotions. The base
branch for contributor PRs is **always `develop`** unless a maintainer
directs otherwise.

### 2. Fill in the PR template

The [pull request template](.github/PULL_REQUEST_TEMPLATE.md) asks for a
summary, linked issues, the type of change, and a slot for `ctest` output.
Please fill it in — incomplete PR bodies slow down review.

### 3. Positioning-regression guard

If your change touches the positioning pipeline (any file under
`src/pos/`, `src/clas/`, `src/madoca/`, `src/rtcm/`, `src/stream/`, or
signal/observation handling) you must either:

- Check the **"Not applicable"** box in the PR template and justify why, or
- Run the relevant reference dataset and state the observed Fix rate and
  accuracy in the PR body, comparing against `develop` before your change.

The PPC benchmark command is documented in the project's backlog notes;
the 62-test CTest suite exercises PPP / PPP-AR / PPP-RTK / VRS-RTK / SPP /
RTK / CLAS / MADOCA paths on canonical inputs. A green CTest run is
necessary but not always sufficient — be explicit about what you verified.

### 4. Respond to review

Reviews are primarily GitHub-based. Address review comments by adding
**new commits** to the PR rather than force-pushing over the history;
maintainers squash on merge so the branch's granular history is not
preserved regardless. Force-push only when a maintainer asks you to rebase.

### 5. Merge path

Merge to `develop` happens once the PR has:

- A green CI run (build + docs workflows; label-sync only runs on `main`)
- Passing `ctest`
- At least one maintainer approval
- Positioning-regression evidence if applicable

Promotion from `develop` to `main` is a separate PR created by a
maintainer when a release is cut. See the
[intake-workflow memory](.github/workflows/) for the two-stage model in
detail.

---

## Labels

Labels are managed declaratively in [`.github/labels.yml`](.github/labels.yml)
and synced to GitHub by a workflow on push to `main`. Do **not** create or
edit labels through the GitHub UI — the sync action is additive, so edits
you make by hand get clobbered on the next run.

The label scheme uses six orthogonal axes. Issues and PRs typically carry
one label from each relevant axis.

| Axis | Values | Prefix? |
|------|--------|---------|
| Type | `bug`, `enhancement`, `documentation`, `question`, `refactor`, `regression`, `performance` | no (retained GitHub defaults + gap-fillers) |
| Module | `module:ppp`, `module:rtk`, `module:rtkrcv`, `module:rnx2rtkp`, `module:clas`, `module:madoca`, `module:toml-config`, `module:build` | `module:` |
| Mode | `mode:realtime`, `mode:post-processing` | `mode:` |
| GNSS | `gnss:gps`, `gnss:galileo`, `gnss:qzss`, `gnss:glonass`, `gnss:beidou` | `gnss:` |
| Priority | `priority:high`, `priority:medium`, `priority:low` | `priority:` |
| Status | `status:needs-triage`, `status:confirmed`, `status:blocked`, `status:waiting-for-info` | `status:` |

To propose a new label, open a PR that edits `labels.yml` and explain the
rationale in the PR body.

---

## Licensing of contributions

MRTKLIB is distributed under the [BSD 2-Clause License](LICENSE). By
submitting a pull request, you agree that your contribution may be
distributed under the same license (the "inbound = outbound" convention).
No separate Contributor License Agreement is required.

If your change adds code derived from another project, please make sure
that project's license is compatible with BSD 2-Clause, preserve the
original copyright notice, and note the provenance in the commit message.

---

## Code of Conduct

Please be civil and constructive in all project spaces. See
[CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) for the full expectations and the
reporting process.

---

## Getting help

- General usage questions: open an issue with the "Question / usage help" template.
- Bug reports and feature requests: use the other templates above.
- Security issues: follow [SECURITY.md](SECURITY.md).
- Documentation: <https://h-shiono.github.io/MRTKLIB/>
