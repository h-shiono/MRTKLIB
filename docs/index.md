# MRTKLIB: Modernized RTKLIB for Next-Generation GNSS

**MRTKLIB** is a thread-safe C11 GNSS positioning library built on [RTKLIB](https://www.rtklib.com/). It processes the **open PPP / PPP-RTK correction services broadcast by satellites** — QZSS CLAS and MADOCA-PPP, Galileo HAS, BeiDou PPP-B2b and SouthPAN PVS — as well as IGS products and real-time streams, in one code base, for both post-processing (`mrtk post`) and real-time processing (`mrtk run`).

In the spirit of RTKLIB, it aims to be both a readable reference for how each service is applied and practical software for day-to-day and production use.

<div class="grid cards" markdown>

-   :material-rocket-launch:{ .lg .middle } __Getting Started__

    ---

    Build from source and run your first positioning solution in minutes.

    [:octicons-arrow-right-24: Installation](getting-started/install.md)

-   :material-console:{ .lg .middle } __CLI Reference__

    ---

    Learn the `mrtk` unified command-line interface and its subcommands.

    [:octicons-arrow-right-24: CLI Guide](guide/cli.md)

-   :material-cog:{ .lg .middle } __Configuration__

    ---

    Configure positioning modes, constellations, and correction sources via TOML.

    [:octicons-arrow-right-24: Configuration Guide](guide/configuration.md)

-   :material-book-open-variant:{ .lg .middle } __Reference__

    ---

    Full configuration options reference with all available parameters.

    [:octicons-arrow-right-24: Config Options](reference/config-options.md)

</div>

---

## Key Features

### Changes from RTKLIB

- **Per-instance state** — a context structure (`mrtk_ctx_t`) replaces global variables in the main processing paths, so that several solvers can run in one process
- **POSIX / C11** — no Win32 API; CI builds and runs the regression tests, without the real-time replays, on Linux (Ubuntu, x86-64); the full suite, including the replays, runs on macOS (Apple Silicon)
- **CMake build** — one CMake build; LAPACK via `find_package(LAPACK)`, with an internal LU solver when it is absent
- **Source layout by function** — `src/core/`, `src/pos/`, `src/rtcm/`, `src/models/` and others

### Supported Correction Services

| Service | Delivery | Modes | Post-Processing | Real-Time |
|---------|----------|-------|:-:|:-:|
| **QZSS CLAS** | L6D (CSSR) | PPP-RTK, VRS-RTK | :white_check_mark: | :white_check_mark: |
| **QZSS MADOCA-PPP** | L6E + L6D wide-area iono | PPP, PPP-AR, PPP-AR + iono | :white_check_mark: | :white_check_mark: |
| **Galileo HAS** | E6-B C/NAV | Float PPP | :white_check_mark: | :white_check_mark: |
| **IGS products** | SP3 / CLK / Bias-SINEX | Float PPP, PPP-AR | :white_check_mark: | — |
| **IGS-RTS** | RTCM-SSR over NTRIP | Float PPP | :white_check_mark: | :white_check_mark: |
| **BeiDou PPP-B2b** | BDS-3 GEO B2b-I | Float PPP (BeiDou) | :white_check_mark: | :white_check_mark: |
| **SouthPAN PVS** | SBAS L5 MT32 (PRN 122) | Float PPP | :white_check_mark: | :white_check_mark: |

Constellations, receiver inputs, release history and regression tests per service: [Correction Service Capability Matrix](reference/correction-services.md).

### Lineage

MRTKLIB began by unifying the QZSS augmentation libraries into one code base on a modernized RTKLIB core; that integration is complete:

| Component | Description | Status |
|-----------|-------------|--------|
| **MALIB** | MADOCA-PPP structural base (directory layout, threading, streams) | :white_check_mark: Integrated |
| **MADOCALIB** | PPP/PPP-AR engine, L6E SSR decoder, L6D ionospheric decoder | :white_check_mark: Integrated |
| **CLASLIB** | CLAS PPP-RTK, VRS-RTK, CSSR decoder | :white_check_mark: Integrated |

### Positioning Modes

| Mode | Post-Processing | Real-Time |
|------|:-:|:-:|
| SPP (Single Point) | :white_check_mark: | :white_check_mark: |
| PPP (Precise Point) | :white_check_mark: | :white_check_mark: |
| PPP-AR (Ambiguity Resolution) | :white_check_mark: | :white_check_mark: |
| PPP-AR + Ionosphere | :white_check_mark: | :white_check_mark: |
| PPP-RTK (CLAS) | :white_check_mark: | :white_check_mark: |
| VRS-RTK (CLAS) | :white_check_mark: | — |

---

## Related Projects

Ready-to-use packages for trying MRTKLIB without building it from source:

| Project | Description |
|---------|-------------|
| **[mrtklib-docker-ui](https://github.com/h-shiono/mrtklib-docker-ui)** | Web UI for MRTKLIB in a Docker container — post-processing (`mrtk post`) and real-time positioning (`mrtk run`) from the browser, with TOML import / export and result visualization. No compilation required: `docker compose up`. |
| **[mrtklib-quickstart](https://github.com/h-shiono/mrtklib-quickstart)** ([docs](https://h-shiono.github.io/mrtklib-quickstart/), 日本語 / English) | One-script quickstart for real-time MADOCA-PPP with a Septentrio mosaic-G5 receiver — configures the receiver, launches the Docker web UI, and includes teaching material for Windows / macOS / Linux. |

## Quick Start

```bash
# Build
cmake --preset default
cmake --build build

# Post-processing (PPP)
mrtk post -k conf/madocalib/rnx2rtkp.toml obs.obs nav.nav correction.l6

# Real-time positioning (CLAS PPP-RTK)
mrtk run -s -o conf/claslib/rtkrcv.toml
```

See the [Installation Guide](getting-started/install.md) for detailed build instructions.

---

## License

MRTKLIB is licensed under the [BSD 2-Clause License](https://github.com/h-shiono/MRTKLIB/blob/main/LICENSE).

Upstream components (MALIB, MADOCALIB, CLASLIB, GSILIB) are each licensed under BSD 2-Clause by their respective authors (JAXA, TOSHIBA, Cabinet Office, Lighthouse Technology & Consulting, Mitsubishi Electric, and the Geospatial Information Authority of Japan).
