# MRTKLIB : Modernized RTKLIB for Next-Generation GNSS

[![License](https://img.shields.io/badge/License-BSD_2--Clause-blue.svg)](LICENSE)
[![Build](https://img.shields.io/badge/build-CMake-success.svg)]()
[![C11](https://img.shields.io/badge/standard-C11-blue.svg)]()
[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.20373746.svg)](https://doi.org/10.5281/zenodo.20373746)

**MRTKLIB** is a modern, thread-safe C11 GNSS positioning library built on [RTKLIB](https://www.rtklib.com/), covering the **open PPP / PPP-RTK correction services** — QZSS CLAS and MADOCA-PPP, Galileo HAS, and IGS products and real-time streams — in a single code base, for both post-processing (`mrtk post`) and real-time processing (`mrtk run`).

In the spirit of RTKLIB, it aims to be both a readable reference for how each service is applied and practical software for day-to-day and production use. The CLAS and MADOCA-PPP engines are regression-tested for parity with the official [CLASLIB](https://github.com/QZSS-Strategy-Office/claslib) / [MADOCALIB](https://github.com/QZSS-Strategy-Office/madocalib), and absolute accuracy is checked against independent truth (e.g. IGS SINEX). Standard positioning (SPP, RTK) is maintained alongside, with selected algorithm improvements from [demo5 RTKLIB](https://github.com/rtklibexplorer/RTKLIB).

---

## 🚀 Key Architectural Overhauls (Departure from Legacy)

MRTKLIB is not just another fork; it is a ground-up architectural redesign aimed at modern software engineering standards:

* **Thread-Safe Design:** Introduced the `mrtk_ctx_t` context structure to encapsulate states, error handling, and logging on a per-instance basis, replacing legacy global variables in the major processing pipelines and enabling parallel processing (e.g., handling multiple streams in a single server).
* **POSIX & C11 Pure:** Purged all Win32 API and legacy `#ifdef` macros. The core library is purely POSIX/C11 compliant, ensuring perfect portability across Linux, macOS (Apple Silicon), and embedded ARM/RISC-V devices.
* **Modern Build System (CMake):** Replaced scattered Makefiles with a unified, clean CMake build system. It natively supports standard `find_package(LAPACK)` for hardware-accelerated matrix operations (replacing bundled proprietary Intel MKL binaries).
* **Domain-Driven Directory Structure:** Flat source files have been beautifully categorized into specific domains (`src/core/`, `src/pos/`, `src/rtcm/`, `src/models/`, etc.) for high maintainability.

## 🛰️ Supported Correction Services

| Service | Delivery | Positioning modes | Post (`mrtk post`) | Real-time (`mrtk run`) |
|---------|----------|-------------------|:-:|:-:|
| **QZSS CLAS** | L6D (CSSR) | PPP-RTK; VRS-RTK (`mrtk cssr2rtcm3` for a real-time RTCM3 VRS stream) | ✅ 1ch / 2ch | ✅ 1ch / 2ch |
| **QZSS MADOCA-PPP** | L6E (SSR) + L6D wide-area ionosphere (PRN 200/201) | PPP, PPP-AR, PPP-AR + iono | ✅ | ✅ |
| **Galileo HAS** | E6-B C/NAV | Float PPP | ✅ | ✅ |
| **IGS products** | SP3 / CLK / Bias-SINEX files | Float PPP, integer PPP-AR (OSB phase biases) | ✅ | — |
| **IGS-RTS** | RTCM-SSR over NTRIP | Float PPP | ✅ (recorded stream) | ✅ |
| **BeiDou PPP-B2b** | BDS-3 GEO B2b | PPP | planned (v0.7.13) | planned |
| **SouthPAN PVS** | SouthPAN SBAS GEO | PPP | under evaluation | under evaluation |

The service is selected with the `correction` configuration axis; see the [configuration guide](docs/guide/configuration.md) and the per-service quick starts under [`docs/guide/`](docs/guide/). The CLAS engine also handles single-band configurations (single-frequency signal sets and the Galileo E1/E5b mismatch of u-blox ZED-F9P-class receivers); see [`docs/design/sf-ppp-rtk.md`](docs/design/sf-ppp-rtk.md).

Standard positioning — SPP (C/N0 weighting, IGG-III robust estimation, TDCP) and RTK (demo5 back-ports) — is supported in both post-processing and real time.

### 🧬 Lineage

MRTKLIB began by unifying the QZSS augmentation libraries into one conflict-free code base on a modernized RTKLIB core; that integration is complete:

| Component | Version | Description | Status |
|-----------|---------|-------------|--------|
| **[MALIB](https://github.com/JAXA-SNU/MALIB)** | feature/1.2.0 (`f006a34`) | MADOCA-PPP structural base (directory layout, threading, streams), by JAXA and TOSHIBA | Integrated |
| **[MADOCALIB](https://github.com/QZSS-Strategy-Office/madocalib)** | ver.2.0 (`8091004`) | PPP/PPP-AR engine, L6E SSR decoder, L6D ionospheric decoder | Integrated |
| **[CLASLIB](https://github.com/QZSS-Strategy-Office/claslib)** | ver.0.8.2 (`9e714b9`) | Centimeter Level Augmentation Service (PPP-RTK, VRS-RTK, CSSR decoder) | Integrated |

MRTKLIB was the first open-source implementation of real-time CLAS PPP-RTK positioning. Later releases extended it beyond QZSS to Galileo HAS and IGS corrections.

### Algorithm Improvements

MRTKLIB's positioning engines have been progressively modernized across releases
(community-fork back-ports, new correction sources, tooling). Per-version detail
is in the [CHANGELOG](CHANGELOG.md) and [release notes](docs/releases/):

| Era | Theme |
|-----|-------|
| **v0.4.x** | demo5 RTK / PPP-RTK algorithm port (PAR, `detslp_dop` / `detslp_code`, full-constellation `varerr`, false-fix fixes); real-time CLAS PPP-RTK (1ch / 2ch) |
| **v0.5.x** | TOML configuration; code-quality sweeps (`clang-format`, mandatory braces); signals restructuring; RINEX 4.00 CNAV; `convbin` / `str2str` ports |
| **v0.6.x** | Unified `mrtk` CLI; NTRIP v2; `mrtk cssr2rtcm3` (CSSR→RTCM3); IGS-products float / RTS / integer PPP-AR (the `correction` axis); SPP accuracy; formatter CI gate; GSDC smartphone benchmark; real-time MADOCA-PPP multi-GNSS signal selection |
| **v0.7.x** | Global SSR correction services: Galileo HAS (E6-B C/NAV) float PPP; two-receiver QZSS L6 (mosaic-CLAS / mosaic-G5) with single-SBF MADOCA-PPP; CLAS PPP-RTK accuracy/robustness fixes and an upstream-identical CSSR dump (`mrtk dump --parse`); real-time MADOCA-PPP PPP-AR/IONO (L6D wide-area ionosphere); CLAS PPP-RTK claslib parity closing the storm-day fix-rate gap; TOML configuration redesign with deprecation aliases; CLAS dual-channel transmit-pattern merge in post-processing and real-time; kinematic-RTK slip-detection regression fix; `rtcm_t` memory-footprint refactor (lazy `lclblock_t`: ~1.2 GB → tens of MB per real-time solver); RTCM3 MSM BDS-3 B2b observation support (decoder + encoder); build-independent MADOCA regression tests with absolute checks; QZSS L6 service routing by frame header with QZSS L6 handover regression tests — BeiDou PPP-B2b planned next |

**Latest — v0.7.12:** **QZSS L6 service routing.** QZSS L6 frames are now classified by their header — vendor ID and MADOCA service ID, per IS-QZSS-L6 / MDC / SAS — in the UBX and SBF decoders, raw streams and `mrtk post`, never by PRN or receiver container. This fixes QZS5 (PRN 197) CLAS frames lost to the ionosphere decoder on SBF, MADOCA-PPP ionospheric frames entering the CLAS demux on UBX (NEO-D9C), and MADOCA SSR overwriting CLAS in `format = "l6e"` CLAS mode; QZNMA frames get their own route for the coming CLAS CMA ([#351](https://github.com/h-shiono/MRTKLIB/issues/351)). `mrtk post` reads the claslib 0.9.0 `.l6d` / `.l6e` inputs, **`mrtk l6extract` now names its files by L6 header PRN (`{prefix}.{PRN}.l6d` / `.l6e`; scripts using the old `{prefix}_J{PRN}_*.l6` names must be updated)** ([#362](https://github.com/h-shiono/MRTKLIB/issues/362)), and CLAS drops L6 null messages ([#344](https://github.com/h-shiono/MRTKLIB/issues/344)). The first QZSS L6 handover regression tests replay five real handovers (NEO-D9C two-channel, mosaic-CLAS single-channel including both transmit-pattern changes) in real time and in post-processing, with an absolute-accuracy check ([#372](https://github.com/h-shiono/MRTKLIB/issues/372)); they found a real-time-only fix loss after some handovers, now tracked in [#389](https://github.com/h-shiono/MRTKLIB/issues/389) / [#390](https://github.com/h-shiono/MRTKLIB/issues/390). Large test datasets now come from release assets of [h-shiono/mrtklib-data](https://github.com/h-shiono/mrtklib-data), fetched by ctest on first use ([#386](https://github.com/h-shiono/MRTKLIB/issues/386)). Also: server sockets rebind after a restart ([#367](https://github.com/h-shiono/MRTKLIB/issues/367)), serial writes report their byte count ([#343](https://github.com/h-shiono/MRTKLIB/issues/343)), **64-bit stream byte counters — `strsum()` / `strsvrstat()` take `uint64_t*` (public API change)** ([#368](https://github.com/h-shiono/MRTKLIB/issues/368)). The regression suite is 171 / 171.

**v0.7.11:** **MADOCA regression tests: one standard in every build, plus absolute accuracy for every case.** `madocalib_pppar_ion_check` had failed on every LAPACK build since v0.6.6 while CI (no BLAS) passed only because CMake switched the same test to an 8× looser tolerance. The cause was not nondeterminism or an MRTKLIB regression (0.236 cm vs a same-backend upstream build): the reference came from an upstream Accelerate build that no longer reproduces, and any matrix-backend change flips PPP-AR fix decisions at a few marginal epochs of the dataset (one flip is 17–24 cm). References are now regenerated from upstream as shipped (internal LU) with a fixed, provenance-recording generator; parity tolerances are the same in every build; the PPP-AR parity checks gate the true integer-fix rate (Q=1, over the reference timeline — the old check counted float PPP and missing epochs as fixed); and all four MADOCA cases gain an absolute check against the week-2360 IGS SINEX that contains the data epoch ([#302](https://github.com/h-shiono/MRTKLIB/issues/302), [PR #338](https://github.com/h-shiono/MRTKLIB/pull/338)). No positioning change; the regression suite is 125 / 125.

**v0.7.10:** **RTCM3 MSM: BeiDou-3 B2b observations no longer dropped — decoder and encoder.** The BeiDou MSM signal-ID table (`msm_sig_cmp[]`, inherited byte-identical from MALIB) left RTCM signal IDs 25–27 empty; RTCM 10403.3 Amendment 2 assigns them to the BDS-3 B2b signals `7D` / `7P` / `7Z`, so every B2b cell in a live MSM stream was silently discarded by the decoder (only a level-2 trace). The RTCM3 encoder's `to_sigid()` shares the same table, so RTCM3 output re-encoded from SBF / NovAtel / BINEX input — whose decoders already emit B2b — lost it as well. The three-entry table fix repairs both directions ([#333](https://github.com/h-shiono/MRTKLIB/issues/333), reported by @philippebourcier with RENAG / mosaic-X5 field comparisons; [PR #335](https://github.com/h-shiono/MRTKLIB/pull/335)). New `utest_msm_bds_b2b` — the suite's first MSM encode→decode round-trip, with wire-ID assertions on the MSM header mask — fails on the old table and passes on the new one ([#296](https://github.com/h-shiono/MRTKLIB/issues/296)). Slot note: the decoder's per-band code priority (`7D > 7I`) puts `7D` in the B2 slot and moves a co-present BDS-2 `7I` to an extended slot, the same placement the RINEX reader already produces, so RTCM input now matches the RINEX path. No positioning change in the regression suite — no bundled positioning stream carries BDS MSM, so the gate matches the develop baseline (122 tests, one documented environment-only LAPACK-tolerance failure, `madocalib_pppar_ion_check`); positioning use of B2b is tracked in [#189](https://github.com/h-shiono/MRTKLIB/issues/189) / [#234](https://github.com/h-shiono/MRTKLIB/issues/234).


**Next:** BeiDou PPP-B2b as the next v0.7.x global SSR service; extend the CLAS-style CSSR dump to MADOCA / Galileo HAS / BeiDou B2b ([#229](https://github.com/h-shiono/MRTKLIB/issues/229)); investigate the HAS absolute-accuracy offset ([#215](https://github.com/h-shiono/MRTKLIB/issues/215)); tuned SPP P5/P6 (clock-jump + position EKF) on the GSDC smartphone benchmark ([#165](https://github.com/h-shiono/MRTKLIB/issues/165))

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
| **Real-time** (`mrtk run`) | Single stream (`inpstr3`) | Dual-channel (`inpstr2` + `inpstr3`) | PPP/PPP-AR/PPP-AR+iono, PPP-RTK with 1ch or 2ch CLAS L6D |

* **Real-time CLAS L6D**: Dual-channel support uses stream 3 for L6 ch1 and stream 2 (base slot, unused in PPP-RTK) for L6 ch2.
* **Real-time L6E**: The rtksvr provides a single correction input (`inpstr3`). Multiple QZSS L6E channels (e.g., QZS-3 and QZS-4) are supported when the receiver multiplexes them into one SBF stream.

## 📊 Live Dashboard

Real-time CLAS PPP-RTK positioning performance can be monitored on a public Grafana dashboard:

> **[CLAS Summary Dashboard](https://live.pntmoni.com/)**

The dashboard shows fix rate, ENU accuracy, satellite visibility, and correction age
streamed from an active `mrtk run` session.

---

## 🧰 Related Projects

Ready-to-use packages for trying MRTKLIB without building it from source:

| Project | Description |
|---------|-------------|
| **[mrtklib-docker-ui](https://github.com/h-shiono/mrtklib-docker-ui)** | Web UI for MRTKLIB in a Docker container — post-processing (`mrtk post`) and real-time positioning (`mrtk run`) from the browser, with TOML import / export and result visualization. No compilation required: `docker compose up`. |
| **[mrtklib-quickstart](https://github.com/h-shiono/mrtklib-quickstart)** ([docs](https://h-shiono.github.io/mrtklib-quickstart/), 日本語 / English) | One-script quickstart for real-time MADOCA-PPP with a Septentrio mosaic-G5 receiver — configures the receiver, launches the Docker web UI, and includes teaching material for Windows / macOS / Linux. |

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
