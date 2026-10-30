# MRTKLIB: Modernized RTKLIB for Next-Generation GNSS

**MRTKLIB** is a modern, thread-safe C11 GNSS positioning library built on [RTKLIB](https://www.rtklib.com/), covering the **open PPP / PPP-RTK correction services** — QZSS CLAS and MADOCA-PPP, Galileo HAS, and IGS products and real-time streams — for both post-processing and real-time processing.

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

### Architectural Overhauls

- **Thread-Safe Design** — `mrtk_ctx_t` context structure replaces global variables, enabling parallel processing
- **POSIX & C11 Pure** — No Win32 API; fully portable across Linux, macOS (Apple Silicon), and embedded ARM/RISC-V
- **Modern Build System** — Unified CMake with `find_package(LAPACK)` for hardware-accelerated matrix operations
- **Domain-Driven Layout** — Source organized into `src/core/`, `src/pos/`, `src/rtcm/`, `src/models/`, etc.

### Supported Correction Services

| Service | Delivery | Modes | Post-Processing | Real-Time |
|---------|----------|-------|:-:|:-:|
| **QZSS CLAS** | L6D (CSSR) | PPP-RTK, VRS-RTK | :white_check_mark: | :white_check_mark: |
| **QZSS MADOCA-PPP** | L6E + L6D wide-area iono | PPP, PPP-AR, PPP-AR + iono | :white_check_mark: | :white_check_mark: |
| **Galileo HAS** | E6-B C/NAV | Float PPP | :white_check_mark: | :white_check_mark: |
| **IGS products** | SP3 / CLK / Bias-SINEX | Float PPP, PPP-AR | :white_check_mark: | — |
| **IGS-RTS** | RTCM-SSR over NTRIP | Float PPP | :white_check_mark: | :white_check_mark: |
| **BeiDou PPP-B2b** | BDS-3 GEO B2b | PPP | planned | planned |
| **SouthPAN PVS** | SouthPAN SBAS GEO | PPP | under evaluation | under evaluation |

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
