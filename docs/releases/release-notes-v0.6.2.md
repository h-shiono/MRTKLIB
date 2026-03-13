# Release Notes — v0.6.2

## Documentation Site (MkDocs Material + Doxygen + GitHub Pages)

### Overview

MRTKLIB now has a modern documentation site built with [MkDocs Material](https://squidfunk.github.io/mkdocs-material/), deployed automatically to GitHub Pages. The site includes hand-written user guides, an auto-generated configuration reference, and a linked Doxygen API reference.

### What's New

#### MkDocs Material Documentation Site

- **Modern UI** — Responsive design with dark mode toggle, navigation tabs, sticky headers, and search
- **Landing page** — Feature overview with card grid navigation
- **Installation guide** — Platform-specific build instructions (Ubuntu, macOS, Apple Silicon)
- **First-run tutorial** — Quick-start examples for `mrtk post` and `mrtk run`
- **CLI reference** — Complete documentation of all `mrtk` subcommands and options
- **Configuration guide** — TOML format overview, constellation selection, migration from `.conf`

#### Auto-Generated Configuration Reference

- **177 options** documented across 23 TOML sections
- Generated from `conf2toml.py` MAPPING table via `scripts/docs/gen_config_ref.py`
- Includes type information, enum values, and legacy key mapping
- Stays in sync with code — regenerate with a single command

#### Doxygen API Reference Integration

- Doxygen HTML built separately and merged into the MkDocs site at `/api/`
- `Doxyfile.in` updated: `HTML_OUTPUT` changed from `html` to `api`
- Call graphs and source browsing available in the API reference

#### GitHub Actions Deployment

- `.github/workflows/docs.yaml` — Automated deployment on push to `main`
- Pipeline: generate config reference → build MkDocs → build Doxygen → merge → deploy
- Independent from the existing build/test CI workflow

### Files Added

| File | Description |
|------|-------------|
| `mkdocs.yml` | MkDocs configuration with Material theme |
| `docs/requirements.txt` | Python dependencies for docs build |
| `docs/index.md` | Landing page |
| `docs/getting-started/install.md` | Installation guide |
| `docs/getting-started/first-run.md` | Quick-start tutorial |
| `docs/guide/cli.md` | CLI reference |
| `docs/guide/configuration.md` | TOML configuration guide |
| `docs/reference/config-options.md` | Auto-generated options reference |
| `docs/releases/changelog.md` | Changelog page |
| `docs/stylesheets/extra.css` | Custom CSS |
| `scripts/docs/gen_config_ref.py` | Config reference generator |
| `.github/workflows/docs.yaml` | GitHub Pages deployment |

### Files Changed

| File | Change |
|------|--------|
| `Doxyfile.in` | `HTML_OUTPUT`: `html` → `api` |
| `CMakeLists.txt` | Version 0.6.1 → 0.6.2 |
| `CHANGELOG.md` | v0.6.2 entry added |
| `README.md` | v0.6.2 added to roadmap |
| `.gitignore` | `site/` added |

### Test Results

62/62 tests pass (no regressions).
