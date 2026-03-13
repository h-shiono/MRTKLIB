# MRTKLIB: Modernized RTKLIB for Next-Generation GNSS

**MRTKLIB** is a completely modernized, thread-safe, and modularized C11 library for standard and precise GNSS positioning.

It is designed to overcome the architectural limitations of the original legacy [RTKLIB](https://www.rtklib.com/), providing a robust foundation for next-generation GNSS applications.

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

### QZSS Grand Integration

MRTKLIB unifies the fragmented QZSS augmentation ecosystem into a single, conflict-free library:

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
