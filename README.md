# MRTKLIB : Modernized RTKLIB for Next-Generation GNSS

[![License](https://img.shields.io/badge/License-BSD_2--Clause-blue.svg)](LICENSE)
[![Build](https://img.shields.io/badge/build-CMake-success.svg)]()
[![C11](https://img.shields.io/badge/standard-C11-blue.svg)]()
[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.20373746.svg)](https://doi.org/10.5281/zenodo.20373746)

**MRTKLIB** is a completely modernized, thread-safe, and modularized C11 library for standard and precise GNSS positioning.

It is designed to overcome the architectural limitations of the original legacy [RTKLIB](https://www.rtklib.com/), providing a robust foundation for next-generation GNSS applications, including high-scale server processing, containerized environments, and seamless integration of Japanese QZSS augmentation services.

The structural foundation is based on **[MALIB (MADOCA-PPP Library)](https://github.com/JAXA-SNU/MALIB) feature/1.2.0** developed by JAXA and TOSHIBA. The PPP/PPP-AR positioning engine comes from **[MADOCALIB](https://github.com/QZSS-Strategy-Office/madocalib)**, while the centimetre-level PPP-RTK engine is built on **[CLASLIB](https://github.com/QZSS-Strategy-Office/claslib)** — making MRTKLIB the first open-source implementation to support real-time CLAS PPP-RTK positioning via `rtkrcv`. Kinematic positioning accuracy is further enhanced by selected algorithm improvements from **[demo5 RTKLIB](https://github.com/rtklibexplorer/RTKLIB)**. Both post-processing (`rnx2rtkp`) and real-time processing (`rtkrcv`) are supported, including L6E (SSR orbit/clock/bias) and L6D (CLAS CSSR) correction streams. Beyond the QZSS services, MRTKLIB also supports **IGS-products PPP** via the `correction` axis: float PPP from precise IGS/MGEX files (`igs`) or a real-time IGS-RTS RTCM-SSR stream (`igs-rts`), and integer **PPP-AR** from Bias-SINEX OSB phase biases. It additionally decodes **Galileo HAS** (High Accuracy Service) corrections from the free, global E6-B C/NAV signal for float PPP (`gal-has`) in both post-processing and real-time.

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

MRTKLIB's positioning engines have been progressively modernized across releases
(community-fork back-ports, new correction sources, tooling). Per-version detail
is in the [CHANGELOG](CHANGELOG.md) and [release notes](docs/releases/):

| Era | Theme |
|-----|-------|
| **v0.4.x** | demo5 RTK / PPP-RTK algorithm port (PAR, `detslp_dop` / `detslp_code`, full-constellation `varerr`, false-fix fixes); real-time CLAS PPP-RTK (1ch / 2ch) |
| **v0.5.x** | TOML configuration; code-quality sweeps (`clang-format`, mandatory braces); signals restructuring; RINEX 4.00 CNAV; `convbin` / `str2str` ports |
| **v0.6.x** | Unified `mrtk` CLI; NTRIP v2; `mrtk cssr2rtcm3` (CSSR→RTCM3); IGS-products float / RTS / integer PPP-AR (the `correction` axis); SPP accuracy; formatter CI gate; GSDC smartphone benchmark; real-time MADOCA-PPP multi-GNSS signal selection |
| **v0.7.x** | Global SSR correction services: Galileo HAS (E6-B C/NAV) float PPP; two-receiver QZSS L6 (mosaic-CLAS / mosaic-G5) with single-SBF MADOCA-PPP — BeiDou PPP-B2b planned next |

**Latest — v0.7.1:** Two-receiver QZSS L6 support (mosaic-CLAS / mosaic-G5) and single-SBF MADOCA-PPP. The SBF/UBX raw decoders now route the QZSS L6 stream by the Septentrio SBF **`Source`** field (`1` = L6D, `2` = L6E) instead of by block ID, so both the older generic **`QZSRawL6`** (4069, mosaic-CLAS) and the signal-split **`QZSRawL6D`** (4270) / **`QZSRawL6E`** (4271, mosaic-G5) blocks are handled uniformly. MADOCA L6E SSR carried in a raw SBF stream is now applied in real-time via a dedicated decoder return code that `rtksvr` feeds into the existing `update_ssr` path — so a **single mosaic-G5 SBF stream** (L6D + L6E) drives both `mrtk run` PPP-RTK (CLAS via L6D) **and** MADOCA-PPP (L6E SSR). This also fixes CLAS decoding from a raw mosaic-CLAS SBF stream (the generic 4069 block was previously dropped in `mrtk run` and `mrtk cssr2rtcm3`) and removes an L6E-into-CLAS leak in `mrtk cssr2rtcm3` on mixed mosaic-G5 streams. Confirmed on **mosaic-G5 hardware**: single-SBF MADOCA-PPP applies corrections continuously and converges to ~3 cm horizontal / ~2 cm vertical float std ([#219](https://github.com/h-shiono/MRTKLIB/issues/219), [PR #220](https://github.com/h-shiono/MRTKLIB/pull/220)).

**Next:** BeiDou PPP-B2b as the next v0.7.x global SSR service; investigate the HAS absolute-accuracy offset ([#215](https://github.com/h-shiono/MRTKLIB/issues/215)); tuned SPP P5/P6 (clock-jump + position EKF) on the GSDC smartphone benchmark ([#165](https://github.com/h-shiono/MRTKLIB/issues/165))

> [!NOTE]
> demo5 algorithm improvements are adapted from **[demo5 RTKLIB](https://github.com/rtklibexplorer/RTKLIB)**
> by Tim Everett (rtklibexplorer).  Benchmark results use the
> [PPC-Dataset](https://github.com/taroz/PPC-Dataset) (Taro Suzuki, Chiba Institute of Technology)
> for kinematic positioning and the
> [Google Smartphone Decimeter Challenge 2023](https://www.kaggle.com/competitions/smartphone-decimeter-2023)
> dataset for smartphone SPP.

### Known Limitations

| Mode | L6E (SSR) | L6D (CLAS) | Notes |
|------|-----------|------------|-------|
| **Post-processing** (`mrtk post`) | Multiple `.l6` files | Dual-channel | Full PPP/PPP-AR/PPP-AR+iono/PPP-RTK |
| **Real-time** (`mrtk run`) | Single stream (`inpstr3`) | Dual-channel (`inpstr2` + `inpstr3`) | PPP-RTK with 1ch or 2ch CLAS L6D |

* **Real-time CLAS L6D**: Dual-channel support uses stream 3 for L6 ch1 and stream 2 (base slot, unused in PPP-RTK) for L6 ch2.
* **Real-time L6E**: The rtksvr provides a single correction input (`inpstr3`). Multiple QZSS L6E channels (e.g., QZS-3 and QZS-4) are supported when the receiver multiplexes them into one SBF stream.
* **Real-time PPP-AR+iono**: Ionospheric STEC correction via L6D is available in CLAS PPP-RTK mode but not in the MADOCA PPP-AR+iono path (post-processing only).

## 📊 Live Dashboard

Real-time CLAS PPP-RTK positioning performance can be monitored on a public Grafana dashboard:

> **[CLAS Summary Dashboard](https://live.pntmoni.com/)**

The dashboard shows fix rate, ENU accuracy, satellite visibility, and correction age
streamed from an active `mrtk run` session.

---

## 🛠️ Getting Started (How to Build)

### Prerequisites
* CMake (>= 3.15)
* C11 compatible compiler (GCC, Clang)
* LAPACK/BLAS (Optional but recommended for fast matrix operations. e.g., `liblapack-dev` on Ubuntu, Accelerate framework on macOS) — **requires the LP64 (32-bit integer) interface**; ILP64 providers are not supported (for example, avoid OpenBLAS built with `INTERFACE64=1`). On CMake ≥ 3.22, MRTKLIB requests a 32-bit integer BLAS/LAPACK interface by setting `BLA_SIZEOF_INTEGER=4`. This is a CMake selection hint, not a complete ABI verifier for all BLAS/LAPACK package layouts: if `FindBLAS`/`FindLAPACK` still resolves an ILP64 provider (e.g. an ILP64 OpenBLAS published as plain `libopenblas.so` with no `64`-suffixed name), make an LP64 BLAS/LAPACK provider visible to CMake explicitly. On older CMake, install an LP64 build of your BLAS/LAPACK provider (the default for most distributions).

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

The unified `mrtk` binary will be located in the `build/` directory.

### Running Applications
Configuration files (TOML) are stored in the `conf/` directory.
Test data and regression datasets are available in `tests/data/`.

```bash
# Example: Running post-processing analysis
./build/mrtk post -k conf/malib/rnx2rtkp.toml tests/data/rtklib/rinex/xxxx.obs ...

# Example: Real-time positioning
./build/mrtk run -s -o conf/claslib/rtkrcv.toml
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

## 📚 How to Cite

If you use MRTKLIB in your research or product, please cite it via its archived
release on [Zenodo](https://zenodo.org/). You can cite **all versions** by using
the concept DOI `10.5281/zenodo.20373746`; Zenodo also mints a version-specific
DOI for each individual release.

[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.20373746.svg)](https://doi.org/10.5281/zenodo.20373746)

```bibtex
@software{mrtklib,
  author    = {Shiono, Hayato},
  title     = {{MRTKLIB: Modernized RTKLIB for Next-Generation GNSS}},
  publisher = {Zenodo},
  doi       = {10.5281/zenodo.20373746},
  url       = {https://doi.org/10.5281/zenodo.20373746}
}
```

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

## 🗄️ Benchmark Datasets

**Kinematic (geodetic).** The **PPC-Dataset** (Precise Positioning Challenge
2024), kindly released as open data by:

> **Taro Suzuki**, Chiba Institute of Technology
> <https://github.com/taroz/PPC-Dataset>

**Smartphone (SPP).** The **Google Smartphone Decimeter Challenge 2023**
(GSDC-2023), released on Kaggle by Google for the ION GNSS+ conference and used
here under the Kaggle competition rules:

> <https://www.kaggle.com/competitions/smartphone-decimeter-2023>

See [docs/reference/benchmark.md](docs/reference/benchmark.md) (kinematic) and
[docs/reference/benchmark-gsdc.md](docs/reference/benchmark-gsdc.md) (smartphone)
for usage instructions.
