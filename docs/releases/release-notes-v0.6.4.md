# Release Notes — v0.6.4

## rtkrcv Stability Fixes + Community Profile

**Release date:** 2026-04-16
**Type:** Patch — rtkrcv crash fixes + GitHub Community Profile completion
**Branch:** `release/v0.6.4`

---

### Overview

v0.6.4 is a patch release dominated by rtkrcv stability fixes. It ships three
independently root-caused crashes that affected the `mrtk run` (rtkrcv) status
path, plus the repository-hygiene work that completes MRTKLIB's GitHub
Community Profile (issue/PR templates, declarative labels, CONTRIBUTING,
SECURITY, and Code of Conduct).

No positioning-engine behavior changes. No new public APIs. The 62-test CTest
suite passes unchanged.

---

### rtkrcv stability fixes (user-facing)

#### Fix — status-poll SIGSEGV from out-of-bounds mode[] read (#74)

`prstatus()` had an 8-entry `mode[]` array but newer positioning modes
(`PMODE_PPP_RTK=9`, `PMODE_VRS_RTK=12`, …) had been added since, so the
second status poll in any PPP-RTK / VRS-RTK session read past the array end
and crashed on the invalid pointer. Mode table is now 13 entries covering
`PPP-fixed`, `PPP-RTK`, `SSR2OSR`, `SSR2OSR-fixed`, and `VRS-RTK`, with
bounds checks on both `mode[]` and `freq[]`.

Commit: `5daa212`

#### Fix — status-poll data race on shared state vectors (#74)

`prstatus()` shallow-copied `rtk_t` under the server lock, leaving the
`x / P / xa / Pa` pointers aliased to heap buffers that the processing
thread kept mutating via `rtkpos()`. The race window was ~50–200 ms/epoch
in PPP-RTK mode and surfaced as intermittent SIGSEGVs during long runs.

Fix: extract the values the status path actually needs (position +
covariance diagonal) into local variables while the lock is held, then null
out the shared pointers after unlock so the status path cannot chase them.
A SIGSEGV backtrace handler plus `-rdynamic` on Linux was added to make
any future crashes easier to post-mortem.

Commit: `1656081`

#### Fix — async-signal-safe SIGSEGV handler that preserves core dumps (#82, #85)

The new crash handler originally called `fprintf()` (not async-signal-safe)
and `_exit()` (which suppresses core dumps). It now uses `write(2)` for
diagnostics, reinstalls the default signal handler, and re-raises the
signal so OS core-dump capture still fires. Review rounds on #85 tightened
the handler further.

Commits: `e352c24`, `4dd6b62`, `6e507d0`

---

### Repository hygiene — GitHub Community Profile

v0.6.4 also lands the intake-surface work that activates MRTKLIB's GitHub
Community Profile. These changes do not affect the compiled artifact.

- **Issue and PR templates** (#88) — five issue templates
  (`bug_report`, `positioning_issue`, `feature_request`, `documentation`,
  `question`), a PR template with `ctest` slot and positioning-regression
  check, and `.github/ISSUE_TEMPLATE/config.yml` pointing at the docs site.
- **Declarative labels + sync workflow** (#90) — `.github/labels.yml` is now
  the single source of truth for the 34-label scheme (type / module / mode /
  gnss / priority / status). `EndBug/label-sync` runs on push to `main`
  when `labels.yml` changes.
- **Policy documents** (#92) — `CONTRIBUTING.md` (issue reporting, fork +
  upstream workflow, coding standards, positioning-regression guard),
  `SECURITY.md` (Private Vulnerability Reporting; scope also explicitly
  covers Code of Conduct reports), and `CODE_OF_CONDUCT.md`
  (Contributor Covenant 2.1 verbatim).

Additional: README now links the CLAS real-time Grafana dashboard for users
monitoring `mrtk run` telemetry.

---

### Files Changed

| File | Change |
|------|--------|
| `apps/rtkrcv/rtkrcv.c` | Status-path data-race fix, bounds-checked mode table, async-signal-safe SIGSEGV handler |
| `CMakeLists.txt` | Version 0.6.3 → 0.6.4; `-rdynamic` on Linux for crash backtraces |
| `CONTRIBUTING.md` | New — contributor on-ramp |
| `SECURITY.md` | New — vulnerability disclosure policy |
| `CODE_OF_CONDUCT.md` | New — Contributor Covenant 2.1 |
| `.github/ISSUE_TEMPLATE/*` | New — five issue templates + `config.yml` |
| `.github/PULL_REQUEST_TEMPLATE.md` | New — PR template |
| `.github/labels.yml` | New — declarative label source of truth |
| `.github/workflows/label-sync.yaml` | New — label-sync automation |
| `README.md` | CLAS real-time Grafana dashboard link; v0.6.4 roadmap entry |
| `CHANGELOG.md` | v0.6.4 entry |
| `mkdocs.yml` | v0.6.4 in Releases navigation |

---

### Upgrade notes

- **rtkrcv users on v0.6.3 or earlier** — upgrading is recommended.
  The mode-table out-of-bounds fix (#5daa212) affects anyone running
  `mrtk run` in PPP-RTK or VRS-RTK mode and polling status.
- **No configuration, TOML schema, or API changes.** Existing
  `rtkrcv.toml` / `rnx2rtkp` invocations run unchanged.

---

### Test Results

62/62 tests pass (no regressions).
