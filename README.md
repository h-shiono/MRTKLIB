# MRTKLIB : Modernized RTKLIB for Next-Generation GNSS

[![License](https://img.shields.io/badge/License-BSD_2--Clause-blue.svg)](LICENSE)
[![Build](https://img.shields.io/badge/build-CMake-success.svg)]()
[![C11](https://img.shields.io/badge/standard-C11-blue.svg)]()

**MRTKLIB** is a completely modernized, thread-safe, and modularized C11 library for standard and precise GNSS positioning.

It is designed to overcome the architectural limitations of the original legacy [RTKLIB](https://www.rtklib.com/), providing a robust foundation for next-generation GNSS applications, including high-scale server processing, containerized environments, and seamless integration of Japanese QZSS augmentation services.

The structural foundation is based on **[MALIB (MADOCA-PPP Library)](https://github.com/JAXA-SNU/MALIB) feature/1.2.0** developed by JAXA and TOSHIBA. The PPP/PPP-AR positioning engine comes from **[MADOCALIB](https://github.com/QZSS-Strategy-Office/madocalib)**, while the centimetre-level PPP-RTK engine is built on **[CLASLIB](https://github.com/QZSS-Strategy-Office/claslib)** — making MRTKLIB the first open-source implementation to support real-time CLAS PPP-RTK positioning via `rtkrcv`. Kinematic positioning accuracy is further enhanced by selected algorithm improvements from **[demo5 RTKLIB](https://github.com/rtklibexplorer/RTKLIB)**. Both post-processing (`rnx2rtkp`) and real-time processing (`rtkrcv`) are supported, including L6E (SSR orbit/clock/bias) and L6D (CLAS CSSR) correction streams.

---

## 🚀 Key Architectural Overhauls (Departure from Legacy)

MRTKLIB is not just another fork; it is a ground-up architectural redesign aimed at modern software engineering standards:

* **Thread-Safe Design:** Introduced the `mrtk_ctx_t` context structure to encapsulate states, error handling, and logging on a per-instance basis, replacing legacy global variables in the major processing pipelines and enabling parallel processing (e.g., handling multiple streams in a single server).
* **POSIX & C11 Pure:** Purged all Win32 API and legacy `#ifdef` macros. The core library is purely POSIX/C11 compliant, ensuring perfect portability across Linux, macOS (Apple Silicon), and embedded ARM/RISC-V devices.
* **Modern Build System (CMake):** Replaced scattered Makefiles with a unified, clean CMake build system. It natively supports standard `find_package(LAPACK)` for hardware-accelerated matrix operations (replacing bundled proprietary Intel MKL binaries).
* **Domain-Driven Directory Structure:** Flat source files have been beautifully categorized into specific domains (`src/core/`, `src/pos/`, `src/rtcm/`, `src/models/`, etc.) for high maintainability.

## 🗺️ Roadmap: The QZSS Grand Integration

The ultimate goal is to unify the fragmented QZSS augmentation ecosystem into a single, conflict-free library. Current integration status:

| Component | Version | Description | Status |
|-----------|---------|-------------|--------|
| **MALIB** | feature/1.2.0 (`f006a34`) | MADOCA-PPP structural base (directory layout, threading, streams) | Integrated |
| **MADOCALIB** | ver.2.0 (`8091004`) | PPP/PPP-AR engine, L6E SSR decoder, L6D ionospheric decoder | Integrated |
| **CLASLIB** | ver.0.8.2 (`9e714b9`) | Centimeter Level Augmentation Service (PPP-RTK, VRS-RTK, CSSR decoder) | Integrated |

With the MADOCALIB integration complete, users can process L6E (orbit/clock/bias corrections) and L6D (ionospheric STEC corrections) streams seamlessly in both post-processing and real-time modes.

### Algorithm Improvements

In addition to library integration, selected algorithm improvements from community forks are
incrementally back-ported to each engine:

| Version | Engine | Improvements | Status |
|---------|--------|-------------|--------|
| **v0.4.1** | RTK | demo5 Partial AR (PAR), `detslp_dop` / `detslp_code`, full-constellation `varerr`, false-fix persistence fix | ✅ Released |
| **v0.4.2** | PPP-RTK, PPP | demo5 `detslp_dop` / `detslp_code`, GLONASS clock guard in `ephpos()`, PAR variance gate + arfilter, full-constellation EFACT, adaptive outlier threshold (PPP-RTK only) | ✅ Released |
| **v0.4.3** | PPP-RTK | Real-time CLAS PPP-RTK via `rtkrcv` (BINEX+L6, SBF+L6, RTCM3+UBX; 97.7% fix rate) | ✅ Released |
| **v0.4.4** | PPP-RTK | Dual-channel CLAS real-time via `rtkrcv` (base stream slot repurposed for L6 ch2) | ✅ Released |
| **v0.5.0** | All | TOML configuration (replaces legacy `.conf`); legacy `doc/` removed | ✅ Released |
| **v0.5.1** | PPP-RTK | Bug fix: dual-channel CLAS RT fix rate degradation ([#35](https://github.com/h-shiono/MRTKLIB/issues/35)); `gen_l6_tag.py` tick_scale fix | ✅ Released |
| **v0.5.2** | All | Code quality: mandatory braces on control flow, nested/complex ternary elimination (67 files) | ✅ Released |
| **v0.5.3** | All | Code quality: full `clang-format` application (116 files, Google style) | ✅ Released |
| **v0.5.4** | All | Signals update: frequency / physical band separation and structuring | ✅ Released |
| **v0.5.5** | PPP-RTK | Bug fix: CLAS real-time positioning via UBX does not work ([#31](https://github.com/h-shiono/MRTKLIB/issues/31)) | ✅ Released |
| **v0.5.6** | All | RINEX 4.00 CNAV/CNV2 NAV support (GPS, QZSS, BDS) | ✅ Released |
| **v0.5.x** | — | Port remaining RTKLIB console apps: `convbin`, `str2str` | 💭 Backlog |
| **v0.5.x** | All | Doxygen docstring coverage expansion | 💭 Backlog |
| **v0.6.0** | All | Single CLI App: Unified `mrtk` executable with subcommands | 💭 Backlog |

> [!NOTE]
> **Configuration format change in v0.5.0:** Starting with v0.5.0, all configuration
> files use TOML (`.toml`).  The legacy RTKLIB `key=value` `.conf` format is no longer
> shipped or tested.  `loadopts()` still accepts `.conf` files at runtime, but all
> bundled configs and CTest commands reference `.toml` only.
> If you need to continue using `.conf` files with the original bundled configurations,
> please use the [`support/v0.4.x`](https://github.com/h-shiono/MRTKLIB/tree/support/v0.4.x) branch.

> [!NOTE]
> demo5 algorithm improvements are adapted from **[demo5 RTKLIB](https://github.com/rtklibexplorer/RTKLIB)**
> by Tim Everett (rtklibexplorer).  Benchmark results use the
> [PPC-Dataset](https://github.com/taroz/PPC-Dataset) (Taro Suzuki, Chiba Institute of Technology).

### Known Limitations

| Mode | L6E (SSR) | L6D (CLAS) | Notes |
|------|-----------|------------|-------|
| **Post-processing** (`rnx2rtkp`) | Multiple `.l6` files | Dual-channel | Full PPP/PPP-AR/PPP-AR+iono/PPP-RTK |
| **Real-time** (`rtkrcv`) | Single stream (`inpstr3`) | Dual-channel (`inpstr2` + `inpstr3`) | PPP-RTK with 1ch or 2ch CLAS L6D |

* **Real-time CLAS L6D**: Dual-channel support uses stream 3 for L6 ch1 and stream 2 (base slot, unused in PPP-RTK) for L6 ch2.
* **Real-time L6E**: The rtksvr provides a single correction input (`inpstr3`). Multiple QZSS L6E channels (e.g., QZS-3 and QZS-4) are supported when the receiver multiplexes them into one SBF stream.
* **Real-time PPP-AR+iono**: Ionospheric STEC correction via L6D is available in CLAS PPP-RTK mode but not in the MADOCA PPP-AR+iono path (post-processing only).

---

## 🛠️ Getting Started (How to Build)

### Prerequisites
* CMake (>= 3.15)
* C11 compatible compiler (GCC, Clang)
* LAPACK/BLAS (Optional but recommended for fast matrix operations. e.g., `liblapack-dev` on Ubuntu, Accelerate framework on macOS)

### Build Instructions
MRTKLIB uses standard CMake workflow. Out-of-source builds are strictly recommended.

```bash
# Clone the repository
git clone https://github.com/h-shiono/MRTKLIB.git
cd MRTKLIB

# Configure the project
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build the library and apps
cmake --build build -j
```

Compiled applications (e.g., rnx2rtkp, rtkrcv) will be located in the `build/` directory.

### Running Applications
Configuration files (TOML) are stored in the `conf/` directory.
Test data and regression datasets are available in `tests/data/`.

```bash
# Example: Running post-processing analysis
./build/rnx2rtkp -k conf/malib/rnx2rtkp.toml tests/data/rtklib/rinex/xxxx.obs ...
```

## 👨‍💻 For Developers

### Development Workflow

MRTKLIB is developed using an AI-assisted workflow.
Algorithm porting, test authoring, and code migration are performed with
**[Claude Code](https://claude.ai/claude-code)** (Anthropic).
Architecture, implementation strategy, and final review are directed by the project author;
porting strategy design is also supported by **Gemini Pro** (Google).

All commits, test results, and architectural decisions remain under human authorship and review.

### Running Tests
MRTKLIB includes both robust unit tests (utest) and regression tests to ensure core stability when merging complex GNSS algorithms.

```bash
cd build
ctest --output-on-failure
```

### Generating Documentation
The codebase is fully documented using Doxygen. To generate the API reference and call graphs:

```bash
cmake --build build --target doc
```
Documentation will be generated in `build/doc/html/index.html`.

## 📄 License & Attributions
MRTKLIB is distributed under the BSD 2-Clause License.

This project stands on the shoulders of giants:

| Contributor | Role |
|-------------|------|
| **Tomoji Takasu** | RTKLIB — the foundational GNSS positioning library |
| **Taro Suzuki** | RTKLIB — u-blox receiver decoder |
| **Tim Everett (rtklibexplorer)** | demo5 RTKLIB — kinematic RTK algorithm improvements (PAR, detslp_dop/code, varerr) |
| **Geospatial Information Authority of Japan** | GSILIB v1.0.3 — CLAS grid correction algorithms |
| **Mitsubishi Electric Corp.** | CLASLIB — CLAS PPP-RTK / VRS-RTK engine |
| **Japan Aerospace Exploration Agency** | MALIB — MADOCA-PPP structural base |
| **TOSHIBA ELECTRONIC TECHNOLOGIES** | MALIB + MADOCALIB — MADOCA-PPP engine and L6E/L6D decoder |
| **Cabinet Office, Japan** | MADOCALIB — PPP/PPP-AR positioning algorithms |
| **Lighthouse Technology & Consulting** | MADOCALIB — system integration and L6E/L6D decoder |

For detailed licensing information, please refer to [LICENSE](LICENSE).

## 🗄️ Benchmark Dataset

The kinematic positioning benchmark uses the **PPC-Dataset** (Precise Positioning
Challenge 2024), kindly released as open data by:

> [!NOTE]
> **Taro Suzuki**, Chiba Institute of Technology
> <https://github.com/taroz/PPC-Dataset>

See [docs/benchmark.md](docs/benchmark.md) for usage instructions.
